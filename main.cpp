#include "math.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>
#include <iostream>
#include <GL/glu.h>
#include <vector>
#include <numeric>
#include <fstream>
#include <map>
#include <algorithm>
#include <fstream>
#include <iterator>

#include <boost/optional.hpp>

using boost::optional;

#define JOINTS \
	LeftToe, RightToe, \
	LeftHeel, RightHeel, \
	LeftAnkle, RightAnkle, \
	LeftKnee, RightKnee, \
	LeftHip, RightHip, \
	LeftShoulder, RightShoulder, \
	LeftElbow, RightElbow, \
	LeftWrist, RightWrist, \
	LeftHand, RightHand, \
	LeftFingers, RightFingers, \
	Core, Neck, Head

enum Joint: uint32_t { JOINTS };

constexpr Joint joints[] = { JOINTS };

#undef JOINTS

constexpr uint32_t joint_count = sizeof(joints) / sizeof(Joint);

struct PlayerJoint { unsigned player; Joint joint; };

bool operator==(PlayerJoint a, PlayerJoint b)
{
	return a.player == b.player && a.joint == b.joint;
}

template<typename T> using PerPlayer = std::array<T, 2>;
template<typename T> using PerJoint = std::array<T, joint_count>;

struct JointDef { Joint joint; double radius; bool draggable; };

PerJoint<JointDef> jointDefs =
	{{ { LeftToe, 0.025, false}
	, { RightToe, 0.025, false}
	, { LeftHeel, 0.03, false}
	, { RightHeel, 0.03, false}
	, { LeftAnkle, 0.03, true}
	, { RightAnkle, 0.03, true}
	, { LeftKnee, 0.05, true}
	, { RightKnee, 0.05, true}
	, { LeftHip, 0.10, true}
	, { RightHip, 0.10, true}
	, { LeftShoulder, 0.08, true}
	, { RightShoulder, 0.08, true}
	, { LeftElbow, 0.045, true}
	, { RightElbow, 0.045, true}
	, { LeftWrist, 0.02, false}
	, { RightWrist, 0.02, false}
	, { LeftHand, 0.02, true}
	, { RightHand, 0.02, true}
	, { LeftFingers, 0.02, false}
	, { RightFingers, 0.02, false}
	, { Core, 0.1, false}
	, { Neck, 0.04, false}
	, { Head, 0.11, true}
	}};

template<typename T>
struct PerPlayerJoint: PerPlayer<PerJoint<T>>
{
	using PerPlayer<PerJoint<T>>::operator[];

	T & operator[](PlayerJoint i) { return this->operator[](i.player)[i.joint]; }
	T const & operator[](PlayerJoint i) const { return operator[](i.player)[i.joint]; }
};

using Position = PerPlayerJoint<V3>;

std::array<PlayerJoint, joint_count * 2> make_playerJoints()
{
	std::array<PlayerJoint, joint_count * 2> r;
	unsigned i = 0;
	for (unsigned player = 0; player != 2; ++player)
		for (auto j : joints)
			r[i++] = {player, j};
	return r;
}

const auto playerJoints = make_playerJoints();

struct PlayerDef { V3 color; };

PerPlayer<PlayerDef> playerDefs = {{ {red}, {blue} }};

struct Segment
{
	std::array<Joint, 2> ends;
	double length; // in meters
	bool visible;
};

const Segment segments[] =
	{ {{LeftToe, LeftHeel}, 0.23, true}
	, {{LeftToe, LeftAnkle}, 0.18, true}
	, {{LeftHeel, LeftAnkle}, 0.09, true}
	, {{LeftAnkle, LeftKnee}, 0.42, true}
	, {{LeftKnee, LeftHip}, 0.47, true}
	, {{LeftHip, Core}, 0.28, true}
	, {{Core, LeftShoulder}, 0.38, true}
	, {{LeftShoulder, LeftElbow}, 0.29, true}
	, {{LeftElbow, LeftWrist}, 0.26, true}
	, {{LeftWrist, LeftHand}, 0.08, true}
	, {{LeftHand, LeftFingers}, 0.08, true}
	, {{LeftWrist, LeftFingers}, 0.14, false}

	, {{RightToe, RightHeel}, 0.23, true}
	, {{RightToe, RightAnkle}, 0.18, true}
	, {{RightHeel, RightAnkle}, 0.09, true}
	, {{RightAnkle, RightKnee}, 0.42, true}
	, {{RightKnee, RightHip}, 0.47, true}
	, {{RightHip, Core}, 0.28, true}
	, {{Core, RightShoulder}, 0.38, true}
	, {{RightShoulder, RightElbow}, 0.29, true}
	, {{RightElbow, RightWrist}, 0.26, true}
	, {{RightWrist, RightHand}, 0.08, true}
	, {{RightHand, RightFingers}, 0.08, true}
	, {{RightWrist, RightFingers}, 0.14, false}

	, {{LeftShoulder, RightShoulder}, 0.4, false}
	, {{LeftHip, RightHip}, 0.25, false}

	, {{LeftShoulder, Neck}, 0.23, true}
	, {{RightShoulder, Neck}, 0.23, true}
	, {{Neck, Head}, 0.15, false}
	};

inline void glVertex(V3 const & v) { glVertex3d(v.x, v.y, v.z); }
inline void glNormal(V3 const & v) { glNormal3d(v.x, v.y, v.z); }
inline void glTranslate(V3 const & v) { glTranslated(v.x, v.y, v.z); }
inline void glColor(V3 v) { glColor3d(v.x, v.y, v.z); }

using Player = PerJoint<V3>;

Player spring(Player const & p, optional<Joint> fixed_joint = boost::none)
{
	Player r = p;

	for (auto && j : joints)
	{
		if (j == fixed_joint) continue;

		auto f = [](double distance)
			{
				return std::max(-.3, std::min(.3, distance / 3 + distance * distance * distance));
			};

		for (auto && s : segments)
		{
			if (s.ends[0] == j)
			{
				double force = f(s.length - distance(p[s.ends[1]], p[s.ends[0]]));
				if (std::abs(force) > 0.001)
				{
					V3 dir = normalize(p[s.ends[1]] - p[s.ends[0]]);
					r[j] -= dir * force;
				}
			}
			else if (s.ends[1] == j)
			{
				double force = f(s.length - distance(p[s.ends[1]], p[s.ends[0]]));
				if (std::abs(force) > 0.001)
				{
					V3 dir = normalize(p[s.ends[0]] - p[s.ends[1]]);
					r[j] -= dir * force;
				}
			}
		}

		r[j].y = std::max(jointDefs[j].radius, r[j].y);
	}

	return r;
}

void spring(Position & pos, optional<PlayerJoint> j = boost::none)
{
	for (unsigned player = 0; player != 2; ++player)
	{
		optional<Joint> fixed_joint;
		if (j && j->player == player) fixed_joint = j->joint;

		pos[player] = spring(pos[player], fixed_joint);
	}
}

void triangle(V3 a, V3 b, V3 c)
{
	glVertex(a);
	glVertex(b);
	glVertex(c);
}

void pillar(V3 from, V3 to, double from_radius, double to_radius, unsigned faces)
{
	V3 a = normalize(cross(to - from, V3{1,1,1} - from));
	V3 b = normalize(cross(to - from, a));

	double s = 2 * M_PI / faces;

	glBegin(GL_TRIANGLES);

	for (unsigned i = 0; i != faces; ++i)
	{
		glNormal(a * sin(i * s) + b * cos(i * s));
			glVertex(from + a * from_radius * std::sin( i    * s) + b * from_radius * std::cos( i    * s));
		glVertex(to   + a *   to_radius * std::sin( i    * s) + b *   to_radius * std::cos( i    * s));

		glNormal(a * sin((i + 1) * s) + b * cos((i + 1) * s));
		glVertex(from + a * from_radius * std::sin((i+1) * s) + b * from_radius * std::cos((i+1) * s));

		glVertex(from + a * from_radius * std::sin((i+1) * s) + b * from_radius * std::cos((i+1) * s));
		glVertex(to   + a *   to_radius * std::sin((i+1) * s) + b *   to_radius * std::cos((i+1) * s));

		glNormal(a * sin(i * s) + b * cos(i * s));
		glVertex(to   + a *   to_radius * std::sin( i    * s) + b *   to_radius * std::cos( i    * s));
	}

	glEnd();

}

void render(Player const & p, Segment const s)
{
	auto const a = s.ends[0], b = s.ends[1];
	pillar(p[a], p[b], jointDefs[a].radius, jointDefs[b].radius, 30);
}

void renderWires(Player const & player)
{
	glLineWidth(50);

	for (auto && s : segments) if (s.visible) render(player, s);
}

void renderShape(Player const & player)
{
	auto && j = player;

	auto t = [&](Joint a, Joint b, Joint c)
		{
			glNormal(normalize(cross(j[b] - j[a], j[c] - j[a])));
			triangle(j[a], j[b], j[c]);
		};

	auto crotch = (j[LeftHip] + j[RightHip]) / 2;

	glBegin(GL_TRIANGLES);
		t(LeftAnkle, LeftToe, LeftHeel);
		t(RightAnkle, RightToe, RightHeel);
		t(LeftHip, RightHip, Core);
		t(Core, LeftShoulder, Neck);
		t(Core, RightShoulder, Neck);
		triangle(j[LeftKnee], crotch, j[LeftHip]);
		triangle(j[RightKnee], crotch, j[RightHip]);
	glEnd();
}

void render(Position const & pos, V3 acolor, V3 bcolor, bool ghost = false)
{
	Player const & a = pos[0], & b = pos[1];

	glColor(acolor);
	renderWires(a);

	glColor(bcolor);
	renderWires(b);
}

struct Sequence
{
	std::string description;
	std::vector<Position> positions; // invariant: .size()>=2
};

char const base62digits[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static_assert(sizeof(base62digits) == 62 + 1, "hm");

int fromBase62(char c)
{
	if (c >= 'a' && c <= 'z') return c - 'a';
	if (c >= 'A' && c <= 'Z') return (c - 'A') + 26;
	if (c >= '0' && c <= '9') return (c - '0') + 52;
	
	throw std::runtime_error("not a base 62 digit: " + std::string(1, c));
}

std::ostream & operator<<(std::ostream & o, Position const & p)
{
	auto g = [&o](double const d)
		{
			int const i = int(d * 1000);
			assert(i >= 0);
			assert(i < 4000);
			o << base62digits[i / 62] << base62digits[i % 62];
		};

	for (auto j : playerJoints)
	{
		g(p[j].x + 2);
		g(p[j].y);
		g(p[j].z + 2);
	}

	return o;
}

Position decodePosition(std::string s)
{
	if (s.size() != 2 * joint_count * 3 * 2)
		throw std::runtime_error("position string has incorrect size");

	auto g = [&s]
		{
			double d = double(fromBase62(s[0]) * 62 + fromBase62(s[1])) / 1000;
			s.erase(0, 2);
			return d;
		};

	Position p;

	for (auto j : playerJoints)
		p[j] = {g() - 2, g(), g() - 2};

	return p;
}

std::istream & operator>>(std::istream & i, std::vector<Sequence> & v)
{
	std::string line;

	while(std::getline(i, line))
		if (line.empty() or line.front() == '#')
			continue;
		else if (line.front() == ' ')
		{
			if (v.empty()) throw std::runtime_error("file contains position without preceding description");

			while (line.front() == ' ') line.erase(0, 1);

			v.back().positions.push_back(decodePosition(line));
		}
		else
		{
			v.push_back(Sequence{line});
			std::cout << "Loading: " << line << '\n';
		}

	return i;
}

std::ostream & operator<<(std::ostream & o, Sequence const & s)
{
	o << s.description << '\n';
	for (auto && p : s.positions) o << "    " << p << '\n';
	return o;
}

std::vector<Sequence> load(std::string const filename)
{
	std::vector<Sequence> r;
	std::ifstream ff("positions.txt");
	ff >> r;
	return r;
}

// state

std::vector<Sequence> sequences = load("positions.txt");
unsigned current_sequence = 0;
unsigned current_position = 0; // index into current sequence

PlayerJoint closest_joint = {0, LeftAnkle};

struct NextPos
{
	double howfar;
	unsigned sequence;
	unsigned position;
};

optional<NextPos> next_pos;

optional<PlayerJoint> chosen_joint;
GLFWwindow * window;
bool edit_mode = false;
Position clipboard;

void save(std::string const filename)
{
	std::ofstream f(filename);
	std::copy(sequences.begin(), sequences.end(), std::ostream_iterator<Sequence>(f));
}

class Camera
{
	V2 viewportSize;
	V3 orientation{0, -0.7, 1.7};
		// x used for rotation over y axis, y used for rotation over x axis, z used for zoom
	M proj, mv, full;

	void computeMatrices()
	{
		proj = perspective(90, viewportSize.x / viewportSize.y, 0.1, 6);
		mv = translate(0, 0, -orientation.z) * xrot(orientation.y) * yrot(orientation.x);
		full = proj * mv;
	}

public:

	Camera() { computeMatrices(); }

	M const & projection() const { return proj; }
	M const & model_view() const { return mv; }

	V2 world2xy(V3 v) const
	{
		auto cs = full * V4(v, 1);
		return xy(cs) / cs.w;
	}

	void setViewportSize(int x, int y)
	{
		viewportSize.x = x;
		viewportSize.y = y;
		computeMatrices();
	}

	void rotateHorizontal(double radians)
	{
		orientation.x += radians;
		computeMatrices();
	}

	void rotateVertical(double radians)
	{
		orientation.y += radians;
		orientation.y = std::max(-M_PI/2, orientation.y);
		orientation.y = std::min(M_PI/2, orientation.y);
		computeMatrices();
	}

	void zoom(double z)
	{
		orientation.z += z;
		computeMatrices();
	}

	double getHorizontalRotation() const { return orientation.x; }

} camera;

void grid()
{
	glColor3f(0.5,0.5,0.5);
	glLineWidth(2);
	glBegin(GL_LINES);
		for (double i = -4; i <= 4; ++i)
		{
			glVertex3f(i/2, 0, -2);
			glVertex3f(i/2, 0, 2);
			glVertex3f(-2, 0, i/2);
			glVertex3f(2, 0, i/2);
		}
	glEnd();
}

Sequence & sequence() { return sequences[current_sequence]; }
Position & position() { return sequence().positions[current_position]; }

Position between(Position const & a, Position const & b, double s = 0.5 /* [0,1] */)
{
	Position r;
	for (auto j : playerJoints) r[j] = a[j] + (b[j] - a[j]) * s;
	return r;
}

void add_position()
{
	if (current_position == sequence().positions.size() - 1)
	{
		sequence().positions.push_back(position());
	}
	else
	{
		auto p = between(position(), sequence().positions[current_position + 1]);
		for(int i = 0; i != 50; ++i)
			spring(p);
		sequence().positions.insert(sequence().positions.begin() + current_position + 1, p);
	}
	++current_position;
}

Position * prev_position()
{
	if (current_position != 0) return &sequence().positions[current_position - 1];
	if (current_sequence != 0) return &sequences[current_sequence - 1].positions.back();
	return nullptr;
}

Position * next_position()
{
	if (current_position != sequence().positions.size() - 1) return &sequence().positions[current_position + 1];
	if (current_sequence != sequences.size() - 1) return &sequences[current_sequence + 1].positions.front();
	return nullptr;
}

void gotoSequence(unsigned const seq)
{
	if (current_sequence != seq)
	{
		current_sequence = seq;

		std::cout << "Seq: " << sequence().description << std::endl;
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_C) // copy
	{
		clipboard = position();
		return;
	}

	if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_V) // paste
	{
		position() = clipboard;
		return;
	}

	if (action == GLFW_PRESS)
		switch (key)
		{
			case GLFW_KEY_INSERT: add_position(); break;

			case GLFW_KEY_PAGE_UP:
				if (current_sequence != 0)
				{
					gotoSequence(current_sequence - 1);
					current_position = 0;
				}
				break;

			case GLFW_KEY_PAGE_DOWN:
				if (current_sequence != sequences.size() - 1)
				{
					gotoSequence(current_sequence + 1);
					current_position = 0;
				}
				break;

			// set position to prev/next/center

			case GLFW_KEY_Y: if (auto p = prev_position()) position() = *p; break;
			case GLFW_KEY_I: if (auto p = next_position()) position() = *p; break;
			case GLFW_KEY_U:
				if (auto next = next_position())
				if (auto prev = prev_position())
				{
					position() = between(*prev, *next);
					for(int i = 0; i != 6; ++i) spring(position());
				}
				break;

			// set joint to prev/next/center

			case GLFW_KEY_H: if (auto p = prev_position()) position()[closest_joint] = (*p)[closest_joint]; break;
			case GLFW_KEY_K: if (auto p = next_position()) position()[closest_joint] = (*p)[closest_joint]; break;
			case GLFW_KEY_J:
				if (auto next = next_position())
				if (auto prev = prev_position())
					position()[closest_joint] = ((*prev)[closest_joint] + (*next)[closest_joint]) / 2;
					for(int i = 0; i != 6; ++i) spring(position());
				break;

			// new sequence

			case GLFW_KEY_N:
			{
				auto p = position();
				sequences.push_back(Sequence{"new", {p, p}});
				current_sequence = sequences.size() - 1;
				current_position = 0;
				break;
			}

			case GLFW_KEY_V: edit_mode = !edit_mode; break;

			case GLFW_KEY_S: save("positions.dat"); break;
			case GLFW_KEY_DELETE:
			{
				if (mods & GLFW_MOD_CONTROL)
				{
					if (sequences.size() > 1)
					{
						sequences.erase(sequences.begin() + current_sequence);
						if (current_sequence == sequences.size()) --current_sequence;
						current_position = 0;
					}
				}
				else
				{
					if (sequence().positions.size() > 2)
					{
						sequence().positions.erase(sequence().positions.begin() + current_position);
						if (current_position == sequence().positions.size()) --current_position;
					}
				}

				break;
			}
		}
}

double dist(Player const & a, Player const & b)
{
	double d = 0;
	for (auto && j : joints)
		d += distance(a[j], b[j]);
	return d;
}

double dist(Position const & a, Position const & b)
{
	return dist(a[0], b[0]) + dist(a[1], b[1]);
}

struct Viable
{
	unsigned sequence;
	unsigned begin, end; // half-open range
};

PerPlayerJoint<std::vector<Viable>> viable;

bool anyViable(PlayerJoint j)
{
	for (auto && v : viable[j]) if (v.end - v.begin > 1) return true;
	return false;
}

void drawJoint(Position const & pos, PlayerJoint pj)
{
	auto color = playerDefs[pj.player].color;
	bool highlight = (pj == (chosen_joint ? *chosen_joint : closest_joint));
	double extraBig = highlight ? 0.01 : 0.005;

	if (edit_mode)
		color = highlight ? yellow : white;
	else if (!anyViable(pj) || (chosen_joint && !highlight))
		extraBig = 0;
	else
		color = highlight
			? white * 0.6 + color * 0.4
			: white * 0.3 + color * 0.7;

	glColor(color);

	glPushMatrix();
		glTranslate(pos[pj]);
		GLUquadricObj * Sphere = gluNewQuadric();

		gluSphere(Sphere, jointDefs[pj.joint].radius + extraBig, 20, 20);

		gluDeleteQuadric(Sphere);
	glPopMatrix();
}

void drawJoints(Position const & pos)
{
	for (auto pj : playerJoints) drawJoint(pos, pj);
}

unsigned explore_forward(unsigned pos, PlayerJoint j, Sequence const & seq)
{
	optional<V3> last;
	optional<V2> lastxy;
	for (; pos != seq.positions.size(); ++pos)
	{
		V3 v = seq.positions[pos][j];
		if (last && distanceSquared(v, *last) < 0.003) break;
		V2 xy = camera.world2xy(v);
		if (lastxy && distanceSquared(xy, *lastxy) < 0.0015) break;
		last = v;
		lastxy = xy;
	}
	return pos;
}

unsigned explore_backward(unsigned upos, PlayerJoint j, Sequence const & seq)
{
	optional<V3> last;
	optional<V2> lastxy;
	int pos = upos;
	for (; pos >= 0; --pos)
	{
		auto && v = seq.positions[pos][j];
		if (last && distanceSquared(v, *last) < 0.003) break;
		auto xy = camera.world2xy(v);
		if (lastxy && distanceSquared(xy, *lastxy) < 0.0015) break;
		last = v;
		lastxy = xy;
	}
	++pos;

	return pos;
}

void determineViables()
{
	for (auto j : playerJoints)
	{
		auto & vv = viable[j];
		vv.clear();
		vv.emplace_back();

		auto & v = vv.back();
		
		v.sequence = current_sequence;

		if (!edit_mode && !jointDefs[j.joint].draggable)
		{
			v.begin = v.end = current_position;
			continue;
		}

		v.end = explore_forward(current_position, j, sequence());
		v.begin = explore_backward(current_position, j, sequence());

		// todo: maintain "no small steps" rule along transition changes

		if (v.end == sequence().positions.size())
			for (unsigned seq = 0; seq != sequences.size(); ++seq)
			{
				auto & s = sequences[seq];
				if (s.positions.front() == sequence().positions.back())
					vv.push_back(Viable{seq, 0, explore_forward(0, j, s)});
				else if (s.positions.back() == sequence().positions.back())
					vv.push_back(Viable{seq, explore_backward(s.positions.size() - 1, j, s), unsigned(s.positions.size())});
			}

		if (v.begin == 0)
			for (unsigned seq = 0; seq != sequences.size(); ++seq)
			{
				auto & s = sequences[seq];
				if (s.positions.back() == sequence().positions.front())
					vv.push_back(Viable{seq, explore_backward(s.positions.size() - 1, j, s), unsigned(s.positions.size())});
				else if (s.positions.front() == sequence().positions.front())
					vv.push_back(Viable{seq, 0, explore_forward(0, j, s)});
			}
	}
}

V2 cursor;

template<typename F>
optional<NextPos> determineNextPos(F distance_to_cursor)
{
	optional<NextPos> np;

	double best = 1000000;

	PlayerJoint const j = chosen_joint ? *chosen_joint : closest_joint;

	for (auto && via : viable[j])
	{
		if (edit_mode && via.sequence != current_sequence) continue;

		auto & seq = sequences[via.sequence].positions;

		for (unsigned pos = via.begin; pos != via.end; ++pos)
		{
			if (via.sequence == current_sequence)
			{
				if (std::abs(int(pos) - int(current_position)) != 1) continue;
			}
			else
			{
				if (position() == seq.back() && pos == seq.size() - 2) ;
				else if (position() == seq.front() && pos == 1) ;
				else continue;
			}

			Position const & n = position();
			Position const & m = sequences[via.sequence].positions[pos];

			V3 p = n[j];
			V3 q = m[j];

			if (distanceSquared(p, q) < 0.001) continue;

			V2 v = camera.world2xy(p);
			V2 w = camera.world2xy(q);

			V2 a = cursor - v;
			V2 b = w - v;

			double howfar = std::max(0., std::min(1., inner_prod(a, b) / norm2(b) / norm2(b)));

			V2 ultimate = v + (w - v) * howfar;

			double d = distanceSquared(ultimate, cursor);

			if (d < best)
			{
				np = NextPos{howfar, via.sequence, pos};
				best = d;
			}
		}
	}

	return np;
}

template<typename F>
void determineNearestJoint(F distance_to_cursor)
{
	double closest = 200;

	for (auto j : playerJoints)
	{
		double d = distance_to_cursor(position()[j]);

		if (d < closest)
		{
			closest = d;
			closest_joint = j;
		}
	}
}

GLfloat light_diffuse[] = {0.5, 0.5, 0.5, 1.0};
GLfloat light_position[] = {1.0, 2.0, 1.0, 0.0};
GLfloat light_ambient[] = {0.3, 0.3, 0.3, 0.0};

void prepareDraw(int width, int height)
{
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(camera.projection().data());

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(camera.model_view().data());
}

void drawViables(PlayerJoint const highlight_joint)
{
	for (auto j : playerJoints)
		for (auto && v : viable[j])
		{
			if (v.end - v.begin < 2) continue;

			auto & seq = sequences[v.sequence].positions;

			if (!(j == highlight_joint)) continue;

			glColor(j == highlight_joint ? yellow : green);

			if (j == highlight_joint || edit_mode)
				glDisable(GL_DEPTH_TEST);

			glBegin(GL_LINE_STRIP);
			for (unsigned i = v.begin; i != v.end; ++i) glVertex(seq[i][j]);
			glEnd();

			if (j == highlight_joint || edit_mode)
			{
				glPointSize(20);
				glBegin(GL_POINTS);
				for (unsigned i = v.begin; i != v.end; ++i) glVertex(seq[i][j]);
				glEnd();
				glEnable(GL_DEPTH_TEST);
			}
		}
}

double jiggle = 0;

int main()
{
	if (!glfwInit())
		return -1;

	window = glfwCreateWindow(640, 480, "Jiu Jitsu Mapper", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	glfwSetKeyCallback(window, key_callback);

	glfwSetMouseButtonCallback(window, [](GLFWwindow *, int button, int action, int mods)
		{
			if (action == GLFW_PRESS) chosen_joint = closest_joint;
			if (action == GLFW_RELEASE) chosen_joint = boost::none;
		});

	glfwSetScrollCallback(window, [](GLFWwindow * window, double xoffset, double yoffset)
		{
			if (yoffset == -1)
			{
				if (current_position != 0) --current_position;
			}
			else if (yoffset == 1)
			{
				if (current_position != sequence().positions.size() - 1) ++current_position;
			}
		});
	
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) camera.rotateVertical(-0.05);
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) camera.rotateVertical(0.05);
		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) { camera.rotateHorizontal(-0.03); jiggle = M_PI; }
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) { camera.rotateHorizontal(0.03); jiggle = 0; }
		if (glfwGetKey(window, GLFW_KEY_HOME) == GLFW_PRESS) camera.zoom(-0.05);
		if (glfwGetKey(window, GLFW_KEY_END) == GLFW_PRESS) camera.zoom(0.05);

		if (!edit_mode)
		{
			jiggle += 0.01;
			camera.rotateHorizontal(sin(jiggle) * 0.0005);
		}

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		camera.setViewportSize(width, height);

		determineViables();

		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		cursor = {((xpos / width) - 0.5) * 2, ((1-(ypos / height)) - 0.5) * 2};


		auto distance_to_cursor = [&](V3 v){ return norm2(camera.world2xy(v) - cursor); };

		next_pos = determineNextPos(distance_to_cursor);

		// editing

		if (chosen_joint && edit_mode && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
		{
			Position new_pos = position();

			V4 const dragger = yrot(-camera.getHorizontalRotation()) * V4{{1,0,0},0};
			
			auto & joint = new_pos[*chosen_joint];

			auto off = camera.world2xy(joint) - cursor;

			joint.x -= dragger.x * off.x;
			joint.z -= dragger.z * off.x;
			joint.y = std::max(jointDefs[chosen_joint->joint].radius, joint.y - off.y);

			spring(new_pos, chosen_joint);

			position() = new_pos;
		}

		determineNearestJoint(distance_to_cursor);
		
		prepareDraw(width, height);

		glEnable(GL_DEPTH);
		glEnable(GL_DEPTH_TEST);

		glNormal3d(0, 1, 0);

		grid();

		Position posToDraw = position();

		PlayerJoint const highlight_joint = chosen_joint ? *chosen_joint : closest_joint;

		if (next_pos && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
		{
			posToDraw = between(
				position(),
				sequences[next_pos->sequence].positions[next_pos->position],
				next_pos->howfar);
		}

		render(posToDraw, red, blue);
		drawJoints(posToDraw);

		glLineWidth(4);

		glNormal3d(0, 1, 0);

		drawViables(highlight_joint);

		if (chosen_joint)
		{
			glDisable(GL_DEPTH_TEST);
			glPointSize(20);
			glBegin(GL_POINTS);
			glColor(white);
			glVertex(posToDraw[*chosen_joint]);
			glEnd();
			glEnable(GL_DEPTH_TEST);
		}

		glfwSwapBuffers(window);
		if (chosen_joint && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && next_pos && next_pos->howfar > 0.7)
		{
			gotoSequence(next_pos->sequence);
			current_position = next_pos->position;
		}
	}

	glfwTerminate();
}

/* Edit guidelines:

Divide & conquer: first define the few key positions along the sequence where you have a clear idea of what all the details ought to look like,
then see where the interpolation gives a silly result, e.g. limbs moving through eachother, and add one or more positions in between.

Don't make many small segments, because it makes making significant adjustments later much harder. Let the interpolation do its work.
Make sure there's always at least one big-ish movement going on, because remember: in view mode, small movements are not shown as draggable.

Stay clear from orthogonality. Keep the jiu jitsu tight, so that you don't have limbs flailing around independently.

Todo:
	- multiple viewports
	- in edit mode, make it possible to temporarily hide a player, to unobstruct the view to the other
	- experiment with different joint editing schemes, e.g. direct joint coordinate component manipulation
	- documentation
	- joint rotation limits
	- prevent joints moving through eachother (maybe, might be more annoying than useful)
	- sticky floor
	- transformed keyframes (so that you can end up in the same position with different offset/rotation)
	- direction signs ("to truck", "zombie")
	- undo
	- enhance viability conditions to cut paths short when their xy projection loops or near-loops
*/

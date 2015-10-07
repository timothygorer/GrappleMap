#ifndef NDEBUG
#define _GLIBCXX_DEBUG
#endif

#include "util.hpp"
#include "camera.hpp"
#include "persistence.hpp"
#include "math.hpp"
#include "positions.hpp"
#include "viables.hpp"
#include "rendering.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>
#include <iostream>
#include <GL/glu.h>
#include <vector>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <fstream>
#include <iterator>

#include <boost/optional.hpp>

using boost::optional;

struct NextPosition
{
	PositionInSequence pis;
	double howfar;
	Reorientation reorientation;
};

double whereBetween(Position const & n, Position const & m, PlayerJoint const j, Camera const & camera, V2 const cursor)
{
	V2 v = world2xy(camera, n[j]);
	V2 w = world2xy(camera, m[j]);

	V2 a = cursor - v;
	V2 b = w - v;

	return std::max(0., /*std::min(1.,*/ inner_prod(a, b) / norm2(b) / norm2(b)/*)*/);
}

optional<NextPosition> determineNextPos(
	PerPlayerJoint<ViablesForJoint> const & viables,
	Graph const & graph, PlayerJoint const j,
	PositionInSequence const from, Reorientation const reorientation,
	Camera const & camera, V2 const cursor, bool const edit_mode)
{
	optional<NextPosition> np;

	double best = 1000000;

	Position const n = apply(reorientation, graph[from]);
	V2 const v = world2xy(camera, n[j]);

	auto consider = [&](PositionInSequence const to, Reorientation const r)
		{
			Position const m = apply(r, graph[to]);

			double const howfar = whereBetween(n, m, j, camera, cursor);

			V2 const w = world2xy(camera, m[j]);

			V2 const ultimate = v + (w - v) * howfar;

			double const d = distanceSquared(ultimate, cursor);

			if (d < best)
			{
				np = NextPosition{to, howfar, r};
				best = d;
			}
		};

	foreach (p : viables[j].viables)
	{
		auto const & viable = p.second;
		PositionInSequence other{p.first, viable.begin};

		if (edit_mode && other.sequence != from.sequence) continue;

		for (; other.position != viable.end; ++other.position)
		{
			if (other.sequence == from.sequence)
			{
				if (std::abs(int(other.position) - int(from.position)) != 1)
					continue;
			}
			else
			{
				if (auto const current_node = node(graph, from))
				{
					auto const & e_to = graph.to(other.sequence);
					auto const & e_from = graph.from(other.sequence);

					if (!(*current_node == e_to.node
							&& other.position == last_pos(graph, other.sequence) - 1) &&
						!(*current_node == e_from.node
							&& other.position == 1))
						continue;
				}
				else continue;
			}
				// todo: clean the above mess up

			consider(other, viable.reorientation);
		}
	}

	return np;
}

// state

Graph graph(load("positions.txt"));
PositionInSequence location;
PlayerJoint closest_joint = {0, LeftAnkle};
optional<PlayerJoint> chosen_joint;
GLFWwindow * window;
bool edit_mode = false;
Position clipboard;
Camera camera;
double jiggle = 0;
Viables viable;
Reorientation reorientation;
optional<NextPosition> next_pos;

Sequence const & sequence() { return graph.sequence(location.sequence); }

void print_status()
{
	std::cout
		<< "\r[" << std::setw(2) << location.position + 1
		<< '/' << std::setw(2) << sequence().positions.size() << "] "
		<< sequence().description << std::string(30, ' ') << std::flush;
}

void key_callback(GLFWwindow *, int key, int /*scancode*/, int action, int mods)
{
	if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_C) // copy
	{
		clipboard = graph[location];
		return;
	}

	if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_V) // paste
	{
		graph.replace(location, clipboard);
		return;
	}

	if (action == GLFW_PRESS)
	{
		switch (key)
		{
			case GLFW_KEY_INSERT: graph.clone(location); break;

			case GLFW_KEY_PAGE_UP:
				if (location.sequence != 0)
				{
					--location.sequence;
					location.position = 0;
					reorientation = noReorientation();
					print_status();
				}
				break;

			case GLFW_KEY_PAGE_DOWN:
				if (location.sequence != graph.num_sequences() - 1)
				{
					++location.sequence;
					location.position = 0;
					reorientation = noReorientation();
					print_status();
				}
				break;

			// set position to center

			case GLFW_KEY_U:
				if (auto nextLoc = next(graph, location))
				if (auto prevLoc = prev(location))
				{
					auto p = between(graph[*prevLoc], graph[*nextLoc]);
					for(int i = 0; i != 30; ++i) spring(p);
					graph.replace(location, p);
				}
				break;

			// set joint to prev/next/center

			case GLFW_KEY_H:
				if (auto p = prev(location))
					replace(graph, location, closest_joint, graph[*p][closest_joint]);
				break;
			case GLFW_KEY_K:
				if (auto p = next(graph, location))
					replace(graph, location, closest_joint, graph[*p][closest_joint]);
				break;
			case GLFW_KEY_J:
				if (auto prevLoc = prev(location))
				if (auto nextLoc = next(graph, location))
				{
					Position p = graph[location];
					p[closest_joint] = (graph[*prevLoc][closest_joint] + graph[*nextLoc][closest_joint]) / 2;
					for(int i = 0; i != 30; ++i) spring(p);
					graph.replace(location, p);
				}
				break;

			case GLFW_KEY_KP_4: graph.replace(location, graph[location] + V3{-0.02, 0, 0}); break;
			case GLFW_KEY_KP_6: graph.replace(location, graph[location] + V3{0.02, 0, 0}); break;
			case GLFW_KEY_KP_8: graph.replace(location, graph[location] + V3{0, 0, -0.02}); break;
			case GLFW_KEY_KP_2: graph.replace(location, graph[location] + V3{0, 0, 0.02}); break;

			case GLFW_KEY_KP_9:
			{
				auto p = graph[location];
				for (auto j : playerJoints) p[j] = xyz(yrot(-0.05) * V4(p[j], 1));
				graph.replace(location, p);
				break;
			}
			case GLFW_KEY_KP_7:
			{
				auto p = graph[location];
				for (auto j : playerJoints) p[j] = xyz(yrot(0.05) * V4(p[j], 1));
				graph.replace(location, p);
				break;
			}

			// new sequence

			case GLFW_KEY_N:
			{
				auto const p = graph[location];
				location.sequence = graph.insert(Sequence{"new", {p, p}});
				location.position = 0;
				break;
			}
			case GLFW_KEY_V: edit_mode = !edit_mode; break;

			case GLFW_KEY_S: save(graph, "positions.txt"); break;

			case GLFW_KEY_DELETE:
			{
				if (mods & GLFW_MOD_CONTROL)
				{
					if (auto const new_seq = graph.erase_sequence(location.sequence))
					{
						location.sequence = *new_seq;
						location.position = 0;
					}
				}
				else if (auto const new_pos = graph.erase(location))
					location.position = *new_pos;

				break;
			}
		}
	}
}

void determineViables()
{
	foreach (j : playerJoints)
		viable[j] = determineViables(graph, location, j, edit_mode, camera, reorientation);
}

GLfloat light_diffuse[] = {0.5, 0.5, 0.5, 1.0};
GLfloat light_ambient[] = {0.3, 0.3, 0.3, 0.0};

void prepareDraw(int width, int height)
{
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	GLfloat light_position[] = {1.0, 2.0, 1.0, 0.0};
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(camera.projection().data());

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(camera.model_view().data());
}

void mouse_button_callback(GLFWwindow *, int button, int action, int /*mods*/)
{
	if (action == GLFW_PRESS) chosen_joint = closest_joint;

	if (action == GLFW_RELEASE)
	{
		chosen_joint = boost::none;

		if (button == GLFW_MOUSE_BUTTON_RIGHT && next_pos && next_pos->howfar > 0.5)
		{
			location = next_pos->pis;
			reorientation = next_pos->reorientation;
			next_pos = boost::none;
		}
	}
}

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

	glfwSetMouseButtonCallback(window, mouse_button_callback);

	glfwSetScrollCallback(window, [](GLFWwindow * /*window*/, double /*xoffset*/, double yoffset)
		{
			if (yoffset == -1)
			{
				if (location.position != 0) --location.position;
			}
			else if (yoffset == 1)
			{
				if (location.position != sequence().positions.size() - 1) ++location.position;
			}

			print_status();
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

		if (!edit_mode && !chosen_joint)
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
		V2 const cursor = {((xpos / width) - 0.5) * 2, ((1-(ypos / height)) - 0.5) * 2};

		if (auto best_next_pos = determineNextPos(
				viable, graph, chosen_joint ? *chosen_joint : closest_joint,
				{location.sequence, location.position}, reorientation, camera, cursor, edit_mode))
		{
			double const speed = 0.08;

			if (next_pos)
			{
				if (next_pos->pis == best_next_pos->pis &&
				    next_pos->reorientation == best_next_pos->reorientation)
					next_pos->howfar += std::max(-speed, std::min(speed, best_next_pos->howfar - next_pos->howfar));
				else if (next_pos->howfar > 0.05)
					next_pos->howfar = std::max(0., next_pos->howfar - speed);
				else
					next_pos = NextPosition{best_next_pos->pis, 0, best_next_pos->reorientation};
			}
			else
				next_pos = NextPosition{best_next_pos->pis, 0, best_next_pos->reorientation};
		}

		Position const reorientedPosition = apply(reorientation, graph[location]);

		Position posToDraw = reorientedPosition;

		// editing

		if (chosen_joint && edit_mode && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
		{
			Position new_pos = graph[location];

			V4 const dragger = yrot(-camera.getHorizontalRotation()) * V4{{1,0,0},0};
			
			auto & joint = new_pos[*chosen_joint];

			auto off = world2xy(camera, apply(reorientation, joint)) - cursor;

			joint.x -= dragger.x * off.x;
			joint.z -= dragger.z * off.x;
			joint.y = std::max(jointDefs[chosen_joint->joint].radius, joint.y - off.y);

			spring(new_pos, chosen_joint);

			graph.replace(location, new_pos);

			posToDraw = apply(reorientation, new_pos);
		}
		else
		{
			if (!chosen_joint)
				closest_joint = *minimal(
					playerJoints.begin(), playerJoints.end(),
					[&](PlayerJoint j) { return norm2(world2xy(camera, reorientedPosition[j]) - cursor); });

			if (next_pos && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
				posToDraw = between(
						reorientedPosition,
						apply(next_pos->reorientation, graph[next_pos->pis]),
						next_pos->howfar);
		}

		auto const center = xz(posToDraw[0][Core] + posToDraw[1][Core]) / 2;

		camera.setOffset(center);

		prepareDraw(width, height);

		glEnable(GL_DEPTH);
		glEnable(GL_DEPTH_TEST);

		glNormal3d(0, 1, 0);
		grid();

		render(viable, posToDraw, chosen_joint ? *chosen_joint : closest_joint, edit_mode);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glLineWidth(4);
		glNormal3d(0, 1, 0);
		drawViables(graph, viable, chosen_joint ? *chosen_joint : closest_joint);

		glDisable(GL_DEPTH_TEST);
		glPointSize(20);
		glColor(white);

		glBegin(GL_POINTS);
		glVertex(posToDraw[chosen_joint ? *chosen_joint : closest_joint]);
		glEnd();

		glfwSwapBuffers(window);

		if (chosen_joint && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && next_pos && next_pos->howfar >= 1)
		{
			location = next_pos->pis;
			reorientation = next_pos->reorientation;
			next_pos = boost::none;
			print_status();
		}
	}

	glfwTerminate();
}
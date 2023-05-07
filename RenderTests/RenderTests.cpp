/*This source code copyrighted by Lazy Foo' Productions (2004-2022)
and may not be redistributed without written permission.*/
#define STB_IMAGE_IMPLEMENTATION
//Using SDL and standard IO
#include <SDL.h>
#include <stdio.h>
#include <iostream>
#include <Eigen/Dense>
#include <time.h>
#include <stb_image.h>

using namespace Eigen;
using namespace std;



Matrix4f rotateMatrix(Vector3f rot);
Matrix4f translateMatrix(Vector3f pos);

//Screen dimension constants
const int SCREEN_WIDTH = 900;
const int SCREEN_HEIGHT = 900;
float border_const = 0.001;

//Main loop flag
bool quit = false;

//Event handler
SDL_Event e;

//Starts up SDL and creates window
bool init();


//Frees media and shuts down SDL
void close();

//The window we'll be rendering to
SDL_Window* gWindow = NULL;

//The surface contained by the window
SDL_Surface* gScreenSurface = NULL;

//The image we will load and show on the screen
SDL_Surface* newscreen = NULL;
float* z_buff = (float*)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
unsigned int* pixels = (unsigned int*) malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);

unsigned int colors[] = { 0xff00ffff, 0x00ffffff, 0xffff00ff, 0x00ff00ff, 0xff0000ff, 0x0000ffff};

int x, y, n;
//int ok = stbi_info("../checkerboard.png", &x, &y, &n);
unsigned char *tex = stbi_load("../textures/checkerboard.png", &x, &y, &n, STBI_rgb);

struct Point2D {
	float x, y, z;
	int sx, sy;
	double ux, uy;
};

struct Camera3D {
	public:
		Vector3f pos;
		Vector3f rot;
		float f;

		void rotate(float alpha, float beta, float gamma) {

			rot = rot + Vector3f{ alpha, beta, gamma };
		}

		void translate(float x, float y, float z) {
			pos += Vector3f{x, y, z};
		}

		Matrix4f getTransform() {
			Matrix4f output;
			output << rotateMatrix(rot) + translateMatrix(pos);
			return output.inverse();
		}

		MatrixXf getCamera() {
			MatrixXf output(3, 4);
			output <<f, 0, 0, 0,
				0, f, 0, 0,
				0, 0, 1, 0;
			return output;
		}

		Vector3f getForward() {
			Matrix4f dog;
			dog << getTransform().inverse();
			return dog(seq(0, 2), 2);
		}
};

struct Object3D {
public:
	Vector3f pos;
	Vector3f rot;
	MatrixXf verticies;
	MatrixXi tris;
	MatrixXf UVs;
	MatrixXi UV_Map;
	unsigned int color;

	void rotate(float alpha, float beta, float gamma) {

		rot = rot + Vector3f{ alpha, beta, gamma };
	}

	void translate(float x, float y, float z) {
		pos += Vector3f{ x, y, z };
	}

	Matrix4f getTransform() {

		Matrix4f output;
		
		output << rotateMatrix(rot) + translateMatrix(pos);
		return output;
	}
};

Matrix4f rotateMatrix(Vector3f rot) {
	MatrixXf rotateZ(4, 4);
	rotateZ << cos(rot(0)), -sin(rot(0)), 0, 0,
		sin(rot(0)), cos(rot(0)), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1;

	MatrixXf rotateY(4, 4);
	rotateY << cos(rot(1)), 0, sin(rot(1)), 0,
		0, 1, 0, 0,
		-sin(rot(1)), 0, cos(rot(1)), 0,
		0, 0, 0, 1;

	MatrixXf rotateX(4, 4);
	rotateX << 1, 0, 0, 0,
		0, cos(rot(2)), sin(rot(2)), 0,
		0, -sin(rot(2)), cos(rot(2)), 0,
		0, 0, 0, 1;
	return rotateZ * rotateY * rotateX;
}

Matrix4f translateMatrix(Vector3f pos) {
	Matrix4f output;
	output << 0, 0, 0, pos(0),
		0, 0, 0, pos(1),
		0, 0, 0, pos(2),
		0, 0, 0, 0;
	return output;
}

unsigned int rgba2hex() {

}

bool init()
{
	//Initialization flag
	bool success = true;

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		success = false;
	}
	else
	{
		//Create window
		gWindow = SDL_CreateWindow("Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
		if (gWindow == NULL)
		{
			printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
			success = false;
		}
		else
		{
			//Get window surface
			gScreenSurface = SDL_GetWindowSurface(gWindow);
		}
	}

	return success;
}

void close()
{
	//Deallocate surface
	SDL_FreeSurface(newscreen);
	newscreen = NULL;

	//Destroy window
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;

	free(pixels);
	free(z_buff);
	//Quit SDL subsystems
	SDL_Quit();
}

void Barycentric(const Point2D& p, const Point2D& a, const Point2D& b, const Point2D& c, float& u, float& v, float& w)
{
	Vector2f v0 = { b.sx - a.sx,b.sy - a.sy }, v1 = { c.sx - a.sx,c.sy - a.sy }, v2 = { p.sx - a.sx,p.sy - a.sy };
	float den = v0[0] * v1[1] - v1[0] * v0[1];
	v = (v2[0] * v1[1] - v1[0] * v2[1]) / den;
	w = (v0[0] * v2[1] - v2[0] * v0[1]) / den;
	u = 1.0f - v - w;
}

void drawTri(const Point2D& v0, const Point2D& v1, const Point2D& v2, unsigned int* canvas, unsigned int color)
{
	// Compute triangle bounding box
	int minX = min({ v0.sx, v1.sx, v2.sx });
	int minY = min({ v0.sy, v1.sy, v2.sy });
	int maxX = max({ v0.sx, v1.sx, v2.sx });
	int maxY = max({ v0.sy, v1.sy, v2.sy });

	// Clip against screen bounds
	minX = max(minX, 0);
	minY = max(minY, 0);
	maxX = min(maxX, SCREEN_WIDTH - 1);
	maxY = min(maxY, SCREEN_HEIGHT - 1);

	// Rasterize
	Point2D p;
	float u, v, w;
	for (p.sy = minY; p.sy <= maxY; p.sy++) {
		for (p.sx = minX; p.sx <= maxX; p.sx++) {
			// Determine barycentric coordinates
			Barycentric(p, v0, v1, v2, u, v, w);
			float z = 0;
			//Barycentric(p, v0, v1, v2, u, v, w);
			// If p is on or inside all edges, render pixel.
			if (u >= 0 && v >= 0 && w >= 0) {//&& u + v + w - 1.f <= 0.1f)
				//printf("%f \n", u+v+ w);

				z = 1/(1/v0.z * u + 1/v1.z * v + 1/v2.z * w);
				if (z < z_buff[p.sx + p.sy * SCREEN_WIDTH]) {
					//cout << z << endl;
					p.ux = (v0.ux * u + v1.ux * v + v2.ux * w) * z;
					p.uy = (v0.uy * u + v1.uy * v + v2.uy * w) * z;
					//cout << p.sx << endl;

					int texX = round(p.ux * x);
					int texY = round(p.uy * y);
					//cout << tex[texX + texY * x] << endl;
					//cout << tex[texX + texY * x] << ' ' << tex[1 + texX + texY * x] << " " << tex[2 + texX + texY * x] << endl;
					
					if (p.ux <= border_const || p.ux >= 1-border_const || p.uy <= border_const || p.uy >= 1-border_const) {
						color = 0x000000ff;
					}
					else {
						//color = (unsigned int)0 << 24 | (unsigned int)round(255.f * p.ux) << 16 | (unsigned int)round(255.f * p.uy) << 8 | (unsigned int)(255);
						color = ((unsigned int)(tex[3*texX + 3*texY * x])) << 24 | ((unsigned int)(tex[1+ 3 * texX + 3 * texY * x])) << 16 | ((unsigned int)(tex[2+ 3 * texX + 3 * texY * x])) << 8 | (unsigned int)(255);
					}
					//color = (unsigned int)0 << 24 | (unsigned int)round(255.f * p.ux) << 16 | (unsigned int)round(255.f * p.uy) << 8 | (unsigned int)(255);
					//color = ((unsigned int)(255*tex[texX + texY * x])) << 24 | ((unsigned int)(255 * tex[texX + texY * x])) << 16 | ((unsigned int)(255 * tex[texX + texY * x])) << 8 | (unsigned int)(255);
					//cout << color << endl;
					canvas[p.sx + p.sy * SCREEN_WIDTH] = color;
					z_buff[p.sx + p.sy * SCREEN_WIDTH] = z;
				}
			}
			
		}
	}
}

void drawtriangle(unsigned int* canvas, Camera3D camera, Object3D object, int tri, unsigned int color) {
	MatrixXf screen_space_coords(3, 4);
	Point2D v0{};
	Point2D v1{};
	Point2D v2{};
	//cout << triangles << endl << endl;
	for (int i = 0; i < 3; i++) {

		screen_space_coords(i, all) << (camera.getCamera() * camera.getTransform() * object.getTransform() * object.verticies(object.tris(tri, i), all).transpose()).transpose();
		//cout << cube.verticies(cube.tris(tri, i),all) << endl;
		switch (i) {
		case 0:
			v0.x = screen_space_coords(i, 0);
			v0.y = screen_space_coords(i, 1);
			v0.z = abs(screen_space_coords(i, 2));
			v0.ux = object.UVs(object.UV_Map(tri, i), 0)/v0.z;
			v0.uy = object.UVs(object.UV_Map(tri, i), 1) / v0.z;
			v0.sx = (int)round(SCREEN_WIDTH * (screen_space_coords(i, 0) / abs(screen_space_coords(i, 2)) + .5));
			v0.sy = (int)round(SCREEN_HEIGHT * (1-(screen_space_coords(i, 1) / abs(screen_space_coords(i, 2)) + .5)));
		case 1:
			v1.x = screen_space_coords(i, 0);
			v1.y = screen_space_coords(i, 1);
			v1.z = abs(screen_space_coords(i, 2));
			v1.ux = object.UVs(object.UV_Map(tri, i), 0)/ v1.z;
			v1.uy = object.UVs(object.UV_Map(tri, i), 1)/ v1.z;
			v1.sx = (int)round(SCREEN_WIDTH * (screen_space_coords(i, 0) / abs(screen_space_coords(i, 2)) + .5));
			v1.sy = (int)round(SCREEN_HEIGHT * (1 - (screen_space_coords(i, 1) / abs(screen_space_coords(i, 2)) + .5)));
		case 2:
			v2.x = screen_space_coords(i, 0);
			v2.y = screen_space_coords(i, 1);
			v2.z = abs(screen_space_coords(i, 2));
			v2.ux = object.UVs(object.UV_Map(tri, i), 0) / v2.z;
			v2.uy = object.UVs(object.UV_Map(tri, i), 1) / v2.z;
			v2.sx = (int)round(SCREEN_WIDTH * (screen_space_coords(i, 0) / abs(screen_space_coords(i, 2)) + .5));
			v2.sy = (int)round(SCREEN_HEIGHT * (1 - (screen_space_coords(i, 1) / abs(screen_space_coords(i, 2)) + .5)));
		}
	}
	
	
	drawTri(v0, v1, v2, canvas, color);
	
}

void drawObject(unsigned int* canvas, Camera3D camera, Object3D object) {
	
	if ((camera.getForward().dot((object.pos - camera.pos).normalized()) >= -0.3)) {
		return;
	}
	for (int tri = 0; tri < object.tris.rows() ; tri++) {
		//cout << tri << endl;
		drawtriangle(pixels, camera, object, tri, object.color);
	}
}

void drawEnv(unsigned int* canvas, Camera3D camera) {
	float horizon = -SCREEN_HEIGHT * sin(camera.rot(2)) + SCREEN_HEIGHT / 2;
	for (int y = 0; y < SCREEN_HEIGHT; ++y)
	{
		for (int x = 0; x < SCREEN_WIDTH; ++x)
		{
			
			if (y < horizon) {
				pixels[x + y * SCREEN_WIDTH] = 0x87CEEBff;
			} else {
				pixels[x + y * SCREEN_WIDTH] = 0x9b7653ff;
			}
		}
	}
}

void clear() {
	for (int y = 0; y < SCREEN_HEIGHT; ++y)
	{
		for (int x = 0; x < SCREEN_WIDTH; ++x)
		{


			pixels[x + y * SCREEN_WIDTH] = 0x000000ff;
			z_buff[x + y * SCREEN_WIDTH] = 0xffffffff;
		}
	}
}

void handleInputs(SDL_Event e, Camera3D Camera) {
	
}

int main(int argc, char* args[])
{	

	MatrixXf Vertexes(8, 4); 
	Vertexes << -.5, -.5, .5, 1,
		.5, -.5, .5, 1,
		.5, .5, .5, 1,
		-.5, .5, .5, 1,
		-.5, -.5, -.5, 1,
		.5, -.5, -.5, 1,
		.5, .5, -.5, 1,
		-.5, .5, -.5, 1;
		

	MatrixXi Triangles(12, 3); 
	Triangles << 0,1,2,
		0,2,3,
		1,5,6,
		1,6,2,
		3,2,6,
		3,6,7,
		5,4,7,
		5,7,6,
		4,0,3,
		4,3,7,
		0,5,1,
		0,4,5;

	MatrixXf UVs(4, 2);
	UVs << 0, 0,
		0, 1,
		1, 0,
		1, 1;

	MatrixXi UV_map(12, 3);
	UV_map << 1,3,2,
		1,2,0,
		1,3,2,
		1,2,0,
		1,3,2,
		1,2,0,
		1,3,2,
		1,2,0,
		1,3,2,
		1,2,0,
		0,3,2,
		0,1,3;

	Object3D cube{ Vector3f{0,0,0}, Vector3f{0,0,0}, Vertexes, Triangles, UVs, UV_map, 0x00ff00ff};
	//Object3D cube2{ Vector3f{-2,0,-3}, Vector3f{0,.5,0}, Vertexes, Triangles, UVs, UV_map, 0x00ff00ff };
	//int Color_data[] = { 1,1,2,2,2,2,2,2,2,2,2,2 };
	
	Camera3D Camera{ Vector3f{0,0,3},Vector3f{0,0,0}, 1 };
	
	float alpha = 0.00;
	float beta = 0.0;
	float gamma = .0;
	
	
	
	
	//Start up SDL and create window
	if (!init())
	{
		printf("Failed to initialize!\n");
	}

	else
	{
		SDL_Renderer* renderer = SDL_CreateRenderer(gWindow,
			-1, SDL_RENDERER_PRESENTVSYNC);

		// Create a streaming texture of size 320 x 240.
		SDL_Texture* screen_texture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
			SCREEN_WIDTH,SCREEN_HEIGHT);

		clear();
		
		drawObject(pixels, Camera, cube);

		clock_t start = clock();
		//While application is running
		while (!quit)
		{
			//Handle events on queue
			while (SDL_PollEvent(&e) != 0)
			{
				//User requests quit
				if (e.type == SDL_QUIT)
				{
					quit = true;
				}
				if (e.type == SDL_KEYDOWN) {
					switch (e.key.keysym.sym)
					{
					case SDLK_LEFT:
						Camera.translate(-.1f, 0.f, 0.f);
						break;

					case SDLK_RIGHT:
						Camera.translate(.1f, 0.f, 0.f);
						break;

					case SDLK_UP:
						Camera.translate(0.f, 0.f, -0.1f);
						break;

					case SDLK_DOWN:
						Camera.translate(0.f, 0.f, 0.1f);
						break;

					case SDLK_a:
						Camera.rotate(0.f, 0.01f, 0.f);
						break;

					case SDLK_d:
						Camera.rotate(0.f, -0.01f, 0.f);
						break;

					case SDLK_w:
						Camera.rotate(0.f, 0.f, -0.01f);
						break;

					case SDLK_s:
						Camera.rotate(0.f, 0.f, 0.01f);
						break;
					case SDLK_SPACE:
						Camera.translate(0.f, 0.2f, 0.f);
						break;
					case SDLK_z:
						Camera.translate(0.f, -0.2f, 0.f);
						break;
					case SDLK_ESCAPE:
						quit = true;
						break;
					default:
						break;
					}
				}
				if (e.type == SDL_KEYDOWN) {
					
				}
			}

			clear();
			drawEnv(pixels, Camera);
			//cout << tex[300] << endl;
			
			drawObject(pixels, Camera, cube); 
			//drawObject(pixels, Camera, cube2);
			
			SDL_UpdateTexture(screen_texture, NULL, pixels, SCREEN_WIDTH * 4);

			SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
			//SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
			SDL_RenderPresent(renderer);
		}
	}

	//Free resources and close SDL
	close();

	return 0;
}
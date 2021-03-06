/*
Copyright (c) 2011, Movania Muhammad Mobeen
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list
of conditions and the following disclaimer in the documentation and/or other
materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.
*/

//A simple cloth using explicit Euler integration based on the SIGGRAPH course notes
//"Realtime Physics" http://www.matthiasmueller.info/realtimephysics/coursenotes.pdf.
//This code is intended for beginners  so that they may understand what is required
//to implement a simple cloth simulation.
//
//This code is under BSD license. If you make some improvements,
//or are using this in your research, do let me know and I would appreciate
//if you acknowledge this in your code.
//
//Controls:
//left click on any empty region to rotate, middle click to zoom
//left click and drag any point to drag it.
//
//Author: Movania Muhammad Mobeen
//        School of Computer Engineering,
//        Nanyang Technological University,
//        Singapore.
//Email : mova0002@e.ntu.edu.sg

#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/freeglut.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> //for matrices
#include <glm/gtc/type_ptr.hpp>


#pragma comment(lib, "glew32.lib")

using namespace std;
const int width = 1024, height = 1024;

int numX = 20, numY=20;
const size_t total_points = (numX+1)*(numY+1);
float fullsize = 4.0f;
float halfsize = fullsize/2.0f;

float timeStep =  1/60.0f;
float currentTime = 0;
double accumulator = timeStep;
int selected_index = -1;
bool bDrawPoints = false;

struct Spring {
	int p1, p2;
	float rest_length;
	float Ks, Kd;
	int type;
};

vector<GLushort> indices;
vector<Spring> springs;

vector<glm::vec3> X;
vector<glm::vec3> V;
vector<glm::vec3> F;

vector<glm::vec3> sumF; //for RK4
vector<glm::vec3> sumV;
 

//for texture mapped lit cloth
struct Vertex {glm::vec3 pos; glm::vec2 uv; glm::vec3 n;}; 
vector<Vertex> vertices; 

int oldX=0, oldY=0;
float rX=15, rY=0;
int state =1 ;
float dist=-23;
const int GRID_SIZE=10;

const int STRUCTURAL_SPRING = 0;
const int SHEAR_SPRING = 1;
const int BEND_SPRING = 2;
int spring_count=0;

const float DEFAULT_DAMPING =  -0.0125f;
float	KsStruct = 0.5f,KdStruct = -0.25f;
float	KsShear = 0.5f,KdShear = -0.25f;
float	KsBend = 0.85f,KdBend = -0.25f;
glm::vec3 gravity=glm::vec3(0.0f,-0.00981f,0.0f);
float mass = 0.5f;

GLint viewport[4];
GLdouble MV[16];
GLdouble P[16];

glm::vec3 Up=glm::vec3(0,1,0), Right, viewDir;

LARGE_INTEGER frequency;        // ticks per second
LARGE_INTEGER t1, t2;           // ticks
double frameTimeQP=0;
float frameTime =0 ;

float startTime =0, fps=0;
int totalFrames=0;


char info[MAX_PATH]={0};

glm::mat4 ellipsoid, inverse_ellipsoid;
int iStacks = 30;
int iSlices = 30;
float fRadius = 1;

// Resolve constraint in object space
glm::vec3 center = glm::vec3(0,0,0); //object space center of ellipsoid
float radius = 1;					 //object space radius of ellipsoid
GLuint textureID;
void StepPhysics(float dt);

void AddSpring(int a, int b, float ks, float kd, int type) {
	Spring spring;
	spring.p1=a;
	spring.p2=b;
	spring.Ks=ks;
	spring.Kd=kd;
	spring.type = type;
	glm::vec3 deltaP = X[a]-X[b];
	spring.rest_length = sqrt(glm::dot(deltaP, deltaP));
	springs.push_back(spring);
}
void OnMouseDown(int button, int s, int x, int y)
{
	if (s == GLUT_DOWN)
	{
		oldX = x;
		oldY = y;
		int window_y = (height - y);
		float norm_y = float(window_y)/float(height/2.0);
		int window_x = x ;
		float norm_x = float(window_x)/float(width/2.0);

		float winZ=0;
		glReadPixels( x, height-y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ );
		if(winZ==1)
			winZ=0;
		double objX=0, objY=0, objZ=0;
		gluUnProject(window_x,window_y, winZ,  MV,  P, viewport, &objX, &objY, &objZ);
		glm::vec3 pt(objX,objY, objZ);
		size_t i=0;
		for(i=0;i<total_points;i++) {
			if( glm::distance(X[i],pt)<0.1) {
				selected_index = i;
				printf("Intersected at %d\n",i);
				break;
			}
		}
	}

	if(button == GLUT_MIDDLE_BUTTON)
		state = 0;
	else
		state = 1;

	if(s==GLUT_UP) {
		selected_index= -1;
		glutSetCursor(GLUT_CURSOR_INHERIT);
	}
}

void OnMouseMove(int x, int y)
{
	if(selected_index == -1) {
		if (state == 0)
			dist *= (1 + (y - oldY)/60.0f);
		else
		{
			rY += (x - oldX)/5.0f;
			rX += (y - oldY)/5.0f;
		}
	} else {
		float delta = 1500/abs(dist);
		float valX = (x - oldX)/delta;
		float valY = (oldY - y)/delta;
		if(abs(valX)>abs(valY))
			glutSetCursor(GLUT_CURSOR_LEFT_RIGHT);
		else
			glutSetCursor(GLUT_CURSOR_UP_DOWN);

		V[selected_index] = glm::vec3(0);
		X[selected_index].x += Right[0]*valX ;
		float newValue = X[selected_index].y+Up[1]*valY;
		if(newValue>0)
			X[selected_index].y = newValue;
		X[selected_index].z += Right[2]*valX + Up[2]*valY;

		//V[selected_index].x = 0;
		//V[selected_index].y = 0;
		//V[selected_index].z = 0;
	}
	oldX = x;
	oldY = y;

	glutPostRedisplay();
}


void DrawGrid()
{
	glBegin(GL_LINES);
	glColor3f(0.5f, 0.5f, 0.5f);
	for(int i=-GRID_SIZE;i<=GRID_SIZE;i++)
	{
		glVertex3f((float)i,0,(float)-GRID_SIZE);
		glVertex3f((float)i,0,(float)GRID_SIZE);

		glVertex3f((float)-GRID_SIZE,0,(float)i);
		glVertex3f((float)GRID_SIZE,0,(float)i);
	}
	glEnd();
}

void InitGL() {
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	 
	GLfloat ambient[4]={0.25f,0.25f,0.25f,0.25f};
	GLfloat diffuse[4]={1,1,1,1};
	GLfloat mat_diffuse[4]={0.5f,0.5f,0.5f,0.5f};
	GLfloat pos[4]={0,0,1,0};
	glLightfv(GL_LIGHT0, GL_POSITION, pos);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_diffuse);

	glDisable(GL_LIGHTING);

	GLubyte data[256];
	for(int j=0;j<16;j++) {
		for(int i=0;i<16;i++) {
			if((i<8 && j<8) || (i>=8 &&j>=8)) 
				data[i+j*16] = 0;
			else
				data[i+j*16] = 255;
		}
	}
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY, 16, 16, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);


	startTime = (float)glutGet(GLUT_ELAPSED_TIME);
	currentTime = startTime;

	// get ticks per second
    QueryPerformanceFrequency(&frequency);

    // start timer
    QueryPerformanceCounter(&t1);
 
	glEnable(GL_DEPTH_TEST);
	int i=0, j=0, count=0;
	int l1=0, l2=0;
	float ypos = 7.0f;
	int v = numY+1;
	int u = numX+1;

	indices.resize( numX*numY*2*3);
	X.resize(total_points);
	vertices.resize(total_points);
	 
	V.resize(total_points);
	F.resize(total_points);

	//for RK4
	sumF.resize(total_points);
	sumV.resize(total_points);

	//fill in X
	for( j=0;j<v;j++) {
		for( i=0;i<u;i++) {
			X[count++] = glm::vec3( ((float(i)/(u-1)) *2-1)* halfsize, fullsize+1, ((float(j)/(v-1) )* fullsize));
		}
	}

	//fill in V
	memset(&(V[0].x),0,total_points*sizeof(glm::vec3));

	//fill in indices
	GLushort* id=&indices[0];
	for (i = 0; i < numY; i++) {
		for (j = 0; j < numX; j++) {
			int i0 = i * (numX+1) + j;
			int i1 = i0 + 1;
			int i2 = i0 + (numX+1);
			int i3 = i2 + 1;
			if ((j+i)%2) {
				*id++ = i0; *id++ = i2; *id++ = i1;
				*id++ = i1; *id++ = i2; *id++ = i3;
			} else {
				*id++ = i0; *id++ = i2; *id++ = i3;
				*id++ = i0; *id++ = i3; *id++ = i1;
			}
		}
	}
	
	//Fill in the uv coordinates
	 
	for(size_t i=0;i<total_points;i++) {
		Vertex& t = vertices[i];
		t.uv.x = (float(X[i].x+halfsize)/ fullsize) *10.0f;	
		t.uv.y = (float(X[i].z)/fullsize) *10.0f; 
	}
	glPointSize(5);

	wglSwapIntervalEXT(0);

	//setup springs
	// Horizontal
	for (l1 = 0; l1 < v; l1++)	// v
		for (l2 = 0; l2 < (u - 1); l2++) {
			AddSpring((l1 * u) + l2,(l1 * u) + l2 + 1,KsStruct,KdStruct,STRUCTURAL_SPRING);
		}

	// Vertical
	for (l1 = 0; l1 < (u); l1++)
		for (l2 = 0; l2 < (v - 1); l2++) {
			AddSpring((l2 * u) + l1,((l2 + 1) * u) + l1,KsStruct,KdStruct,STRUCTURAL_SPRING);
		}


	// Shearing Springs
	for (l1 = 0; l1 < (v - 1); l1++)
		for (l2 = 0; l2 < (u - 1); l2++) {
			AddSpring((l1 * u) + l2,((l1 + 1) * u) + l2 + 1,KsShear,KdShear,SHEAR_SPRING);
			AddSpring(((l1 + 1) * u) + l2,(l1 * u) + l2 + 1,KsShear,KdShear,SHEAR_SPRING);
		}


	// Bend Springs
	for (l1 = 0; l1 < (v); l1++) {
		for (l2 = 0; l2 < (u - 2); l2++) {
			AddSpring((l1 * u) + l2,(l1 * u) + l2 + 2,KsBend,KdBend,BEND_SPRING);
		}
		AddSpring((l1 * u) + (u - 3),(l1 * u) + (u - 1),KsBend,KdBend,BEND_SPRING);
	}
	for (l1 = 0; l1 < (u); l1++) {
		for (l2 = 0; l2 < (v - 2); l2++) {
			AddSpring((l2 * u) + l1,((l2 + 2) * u) + l1,KsBend,KdBend,BEND_SPRING);
		}
		AddSpring(((v - 3) * u) + l1,((v - 1) * u) + l1,KsBend,KdBend,BEND_SPRING);
	}

	//create a basic ellipsoid object
	ellipsoid = glm::translate(glm::mat4(1),glm::vec3(0,2,0));
	ellipsoid = glm::rotate(ellipsoid, 45.0f ,glm::vec3(1,0,0));
	ellipsoid = glm::scale(ellipsoid, glm::vec3(fRadius,fRadius,fRadius/2));
	inverse_ellipsoid = glm::inverse(ellipsoid);
}

void OnReshape(int nw, int nh) {
	glViewport(0,0,nw, nh);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat)nw / (GLfloat)nh, 1.f, 100.0f);

	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetDoublev(GL_PROJECTION_MATRIX, P);

	glMatrixMode(GL_MODELVIEW);
}

void RenderCloth() { 
	glEnableClientState(GL_VERTEX_ARRAY); 
	glEnableClientState(GL_TEXTURE_COORD_ARRAY); 
	glEnableClientState(GL_NORMAL_ARRAY);
 		glVertexPointer(3, GL_FLOAT,  sizeof(Vertex), &vertices[0].pos); 
			glTexCoordPointer(2, GL_FLOAT,sizeof(Vertex), &vertices[0].uv);
				glNormalPointer(GL_FLOAT,  sizeof(Vertex), &vertices[0].n);
					glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT , &indices[0]);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY); 	
}

void OnRender() {
	size_t i=0;
	float newTime = (float) glutGet(GLUT_ELAPSED_TIME);
	frameTime = newTime-currentTime;
	currentTime = newTime;
	//accumulator += frameTime;

	//Using high res. counter
    QueryPerformanceCounter(&t2);
	 // compute and print the elapsed time in millisec
    frameTimeQP = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
	t1=t2;
	accumulator += frameTimeQP;

	++totalFrames;
	if((newTime-startTime)>1000)
	{
		float elapsedTime = (newTime-startTime);
		fps = (totalFrames/ elapsedTime)*1000 ;
		startTime = newTime;
		totalFrames=0;
	}

	sprintf_s(info, "FPS: %3.2f, Frame time (GLUT): %3.4f msecs, Frame time (QP): %3.3f", fps, frameTime, frameTimeQP);
	glutSetWindowTitle(info);
	glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glTranslatef(0,0,dist);
	glRotatef(rX,1,0,0);
	glRotatef(rY,0,1,0);

	glGetDoublev(GL_MODELVIEW_MATRIX, MV);
	viewDir.x = (float)-MV[2];
	viewDir.y = (float)-MV[6];
	viewDir.z = (float)-MV[10];
	Right = glm::cross(viewDir, Up);

	//draw grid
	DrawGrid();

	//draw ellipsoid
	glColor3f(0,1,0);
	glPushMatrix();
		glMultMatrixf(glm::value_ptr(ellipsoid));
			glutWireSphere(fRadius, iSlices, iStacks);
	glPopMatrix();

	 
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_LIGHTING);
		RenderCloth(); 	
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);


	//draw points
	if(bDrawPoints) {
		glBegin(GL_POINTS);
		for(i=0;i<total_points;i++) {
			glm::vec3 p = X[i];
			int is = (i==selected_index);
			glColor3f((float)!is,(float)is,(float)is);
			glVertex3f(p.x,p.y,p.z);
		}
		glEnd();
	}

	glutSwapBuffers();
}

void OnShutdown() {
	X.clear();
	V.clear();
	F.clear();
	vertices.clear(); 

	sumF.clear();
	sumV.clear();
	indices.clear();
	springs.clear();
	glDeleteTextures(1, &textureID);
}

void ComputeForces() {
	size_t i=0;
	for(i=0;i<total_points;i++) {
		F[i] = glm::vec3(0);

		//add gravity force only for non-fixed points
		if(i!=0 && i!=numX	)
			F[i] += gravity;

		//add force due to damping of velocity
		F[i] += DEFAULT_DAMPING*V[i];
	}

	//add spring forces
	for(i=0;i<springs.size();i++) {
		glm::vec3 p1 = X[springs[i].p1];
		glm::vec3 p2 = X[springs[i].p2];
		glm::vec3 v1 = V[springs[i].p1];
		glm::vec3 v2 = V[springs[i].p2];
		glm::vec3 deltaP = p1-p2;
		glm::vec3 deltaV = v1-v2;
		float dist = glm::length(deltaP);

		float leftTerm = -springs[i].Ks * (dist-springs[i].rest_length);
		float rightTerm = springs[i].Kd * (glm::dot(deltaV, deltaP)/dist);
		glm::vec3 springForce = (leftTerm + rightTerm)*glm::normalize(deltaP);

		if(springs[i].p1 != 0 && springs[i].p1 != numX)
			F[springs[i].p1] += springForce;
		if(springs[i].p2 != 0 && springs[i].p2 != numX )
			F[springs[i].p2] -= springForce;
	}
}


void IntegrateEuler(float deltaTime) {
	float deltaTimeMass = deltaTime/ mass;
	size_t i=0;

	for(i=0;i<total_points;i++) {
		glm::vec3 oldV = V[i];
		V[i] += (F[i]*deltaTimeMass);
		X[i]  += deltaTime*oldV;

		if(X[i].y <0) {
			X[i].y = 0;
		}
	}
}
void IntegrateMidpointEuler(float deltaTime) {
	float halfDeltaT = deltaTime/2.0f;
	float deltaTimeMass = halfDeltaT/ mass;
	size_t i=0;
	for( i=0;i<total_points;i++) {
		glm::vec3 oldV = V[i];
		V[i] += (F[i]*deltaTimeMass);
		X[i] += deltaTime*oldV;

	}

	ComputeForces();

	deltaTimeMass = deltaTime/ mass;
	for(i=0;i<total_points;i++) {
		glm::vec3 oldV = V[i];
		V[i] += (F[i]*deltaTimeMass);
		X[i]  += deltaTime*oldV;

		if(X[i].y <0) {
			X[i].y = 0;
		}
	}
}

void IntegrateRK4(float deltaTime) {
	float halfDeltaT  = deltaTime/2.0f;
	float thirdDeltaT = 1/3.0f;
	float sixthDeltaT = 1/6.0f;
	float halfDeltaTimeMass = halfDeltaT/ mass;
	float deltaTimeMass = deltaTime/ mass;
	size_t i=0;

	memset(&sumF[0],0,sumF.size()*sizeof(glm::vec3));
	memset(&sumV[0],0,sumV.size()*sizeof(glm::vec3));

	for(i=0;i<total_points;i++) {
		sumF[i] = (F[i]*halfDeltaTimeMass) * sixthDeltaT;
		sumV[i] = halfDeltaT*V[i]  * sixthDeltaT;
	}

	ComputeForces();

	for(i=0;i<total_points;i++) {
		sumF[i] = (F[i]*halfDeltaTimeMass) * thirdDeltaT;
		sumV[i] = halfDeltaT*V[i]  * thirdDeltaT;
	}

	ComputeForces();

	for(i=0;i<total_points;i++) {
		sumF[i] = (F[i]*deltaTimeMass)   * thirdDeltaT;
		sumV[i] = deltaTime*V[i] * thirdDeltaT;
	}

	ComputeForces();

	for(i=0;i<total_points;i++) {
		sumF[i] = (F[i]*deltaTimeMass) * sixthDeltaT;
		sumV[i] = deltaTime*V[i] * sixthDeltaT;
	}

	for(i=0;i<total_points;i++) {
		V[i] += sumF[i];
		X[i]  += sumV[i];
		if(X[i].y <0) {
			X[i].y = 0;
		}
	}
}

void ApplyProvotDynamicInverse() {

	for(size_t i=0;i<springs.size();i++) {
		//check the current lengths of all springs
		glm::vec3 p1 = X[springs[i].p1];
		glm::vec3 p2 = X[springs[i].p2];
		glm::vec3 deltaP = p1-p2;
		float dist = glm::length(deltaP);
		if(dist>springs[i].rest_length) {
			dist -= (springs[i].rest_length);
			dist /= 2.0f;
			deltaP = glm::normalize(deltaP);
			deltaP *= dist;
			if(springs[i].p1==0 || springs[i].p1 ==numX) {
				V[springs[i].p2] += deltaP;
			} else if(springs[i].p2==0 || springs[i].p2 ==numX) {
			 	V[springs[i].p1] -= deltaP;
			} else {
				V[springs[i].p1] -= deltaP;
				V[springs[i].p2] += deltaP;
			}
		}
	}
}
void EllipsoidCollision() {
	for(size_t i=0;i<total_points;i++) {
		glm::vec4 X_0 = (inverse_ellipsoid*glm::vec4(X[i],1));
		glm::vec3 delta0 = glm::vec3(X_0.x, X_0.y, X_0.z) - center;
		float distance = glm::length(delta0);
		if (distance < 1.0f) {
			delta0 = (radius - distance) * delta0 / distance;

			// Transform the delta back to original space
			glm::vec3 delta;
			glm::vec3 transformInv;
			transformInv = glm::vec3(ellipsoid[0].x, ellipsoid[1].x, ellipsoid[2].x);
			transformInv /= glm::dot(transformInv, transformInv);
			delta.x = glm::dot(delta0, transformInv);
			transformInv = glm::vec3(ellipsoid[0].y, ellipsoid[1].y, ellipsoid[2].y);
			transformInv /= glm::dot(transformInv, transformInv);
			delta.y = glm::dot(delta0, transformInv);
			transformInv = glm::vec3(ellipsoid[0].z, ellipsoid[1].z, ellipsoid[2].z);
			transformInv /= glm::dot(transformInv, transformInv);
			delta.z = glm::dot(delta0, transformInv);
			X[i] +=  delta ;
			V[i] = glm::vec3(0);
		} 
	}
}

void OnIdle() {

	/*
	//Semi-fixed time stepping
	if ( frameTime > 0.0 )
    {
        const float deltaTime = min( frameTime, timeStep );
        StepPhysics(deltaTime );
        frameTime -= deltaTime;
    }
	*/

	//Fixed time stepping + rendering at different fps
	if ( accumulator >= timeStep )
    {
        StepPhysics(timeStep );
        accumulator -= timeStep;
    }

	glutPostRedisplay();
}

void UpdateNormals() {
	size_t i=0;

	//Update positions into our array for rendering
	for( i=0;i<total_points;i++) { 
		vertices[i].pos=X[i] ;  
	}

	for(i=0;i<indices.size();i+=3) {
		glm::vec3 p1 = X[indices[i]];
		glm::vec3 p2 = X[indices[i+1]];
		glm::vec3 p3 = X[indices[i+2]];
		glm::vec3 n  = glm::cross(p2-p1,p3-p1);

		vertices[indices[i]].n    += n/3.0f ; 
		vertices[indices[i+1]].n  += n/3.0f ; 
		vertices[indices[i+2]].n  += n/3.0f ;  
	}

	for(i=0;i<total_points;i++) { 
		glm::vec3& n  = vertices[i].n;
		n = glm::normalize(n);
 	}
}

void StepPhysics(float dt ) {
	ComputeForces();

		//for Explicit/Midpoint Euler
		IntegrateEuler(dt);

		//for mid-point Euler
		//IntegrateMidpointEuler(timeStep);

		//for RK4
		//IntegrateRK4(timeStep);

		EllipsoidCollision();
	ApplyProvotDynamicInverse();
	
	UpdateNormals();
}

void OnKey(unsigned char key, int x, int y) {
	switch(key) {
		case 'm': bDrawPoints=!bDrawPoints;	break;
	}
	glutPostRedisplay();
}
void main(int argc, char** argv) {
	
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowSize(width, height);
	glutCreateWindow("GLUT Cloth Demo [Texture mapped + Lit Explicit Euler Integrated Cloth]");

	glutDisplayFunc(OnRender);
	glutReshapeFunc(OnReshape);
	glutIdleFunc(OnIdle);

	glutMouseFunc(OnMouseDown);
	glutMotionFunc(OnMouseMove);
	glutKeyboardFunc(OnKey);
	glutCloseFunc(OnShutdown);

	glewInit();
	InitGL();

	puts("Press 'm' to toggle disaply of points");
	puts("Use left click of mouse to drag the points");

	glutMainLoop();
}

#ifdef _WIN32
#include<winsock2.h>
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <WinUser.h>


#pragma comment(lib,"ws2_32.lib") //Winsock Library


#ifndef __APPLE__
#  include <GL/glut.h>
#else
#  include <GLUT/glut.h>
#endif
#include <AR/gsub.h>
#include <AR/param.h>
#include <AR/ar.h>
#include <AR/video.h>
#include "object.h"


///
#define COLLIDE_DIST 30000.0

////////////////*udp setup*///////////////
#define BUFLEN 20000  //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data

SOCKET s;
struct sockaddr_in server, si_other;

#define SERVER "127.0.0.1"  //ip address of udp server

SOCKET s2;
struct sockaddr_in client;
int clen = sizeof(client);
char buf2[BUFLEN];
#define PORT2 8887

int slen, recv_len;
char buf[BUFLEN];
WSADATA wsa;
int iResult;
u_long iMode = 10;
slen = sizeof(si_other);
/////////////////////END udp setup //////////////


/* Object Data */
char            *model_name = "Data/object_data2";
ObjectData_T    *object;
int             objectnum;

int             xsize, ysize;
int				thresh = 100;
int             count = 0;

//gl scene 

double  gl_scene[16];

int nFrame = 0;
#define ALMOSTNULL 0.000001
float distX = ALMOSTNULL, distY = ALMOSTNULL, distZ = ALMOSTNULL;
float distX1 = ALMOSTNULL, distY1 = ALMOSTNULL, distZ1 = ALMOSTNULL;
float distX0 = ALMOSTNULL, distY0 = ALMOSTNULL;


/* set up the video format globals */

#ifdef _WIN32
char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
char			*vconf = "";
#endif

char           *cparam_name = "Data/camera_para.dat";
ARParam         cparam;

int up=0;
int anime = 1;
int move = 0;
int redraw = 0;

float sig[500];

/////////function definition ////////////////////

static void   init(void);
static void   cleanup(void);
static void	  update(void);
static void   keyEvent(unsigned char key, int x, int y);
static void   mainLoop(void);

static void udpInit();
static int udpReceive();
static void udpSend(char* message);
static int processMessage(void);
static void prepare_msg(double x_send, double y_send);

static int draw(ObjectData_T *object, int objectnum);
static int  draw_object(int obj_id, double gl_para[16]);

static int draw_scene(void);
static void draw_sin(double x , double y);
static void draw_sqrt(void);
static void draw_saw(void);
static void draw_tri(void);
static void draw_grid(double distX , double distY);
static void draw_line();
static void draw_grid3d(double distX, double distY, double distZ);
static void draw_line3d(double xi, double yi, double zi, double xf, double yf, double zf);
static void draw_controller(double x, double y);
static void draw_signal();


/////////end function definition ////////////////////



int main(int argc, char **argv)
{
	//initialize applications
	memset(buf, '\0', BUFLEN);
	glutInit(&argc, argv);
	init();
	arVideoCapStart();
	glutTimerFunc(100, update, 0);
	//start the main event loop
	argMainLoop(NULL, keyEvent, mainLoop);
	return 0;
}

static void   keyEvent(unsigned char key, int x, int y)
{
	printf(" pressed %c\n", key);
	/* quit if the ESC key is pressed */
	if (key == 0x1b) {
		printf("*** %f (frame/sec)\n", (double)count / arUtilTimer());
		cleanup();
		exit(0);
	}
	else
	if (key == 0x41){
		printf("stuff i guess\n");
	}
}
/*update */
static void update(void){

	char seps[] = " \n";
	char *token1 = NULL;
	char *next_token1 = NULL;
	int i = 0;
		token1 = strtok_s(buf, seps, &next_token1);
		while (token1 != NULL){

			if (token1 != NULL){

				float a = atof(token1);
				sig[i] = a;
				printf("a- %f  ||  sig[i]  %f   ||  token1 %s \n" , a,sig[i] , token1);
				i++;
				token1 = strtok_s(NULL, seps, &next_token1);
			}
		}

	
	glutPostRedisplay(); // Inform GLUT that the display has changed

	//glutTimerFunc(100, update, 0);//Call update after each 25 millisecond

}

/* main loop */
static void mainLoop(void)
{


	 udpReceive();

	if (recv_len != -1){
		processMessage();
	}

	ARUint8         *dataPtr;
	ARMarkerInfo    *marker_info;
	int             marker_num;
	int             i, j, k;

	/* grab a video frame */
	if ((dataPtr = (ARUint8 *)arVideoGetImage()) == NULL) {
		arUtilSleep(2);
		return;
	}

	if (count == 0) arUtilTimerReset();
	count++;
	/*draw the video*/
	argDrawMode2D();
	argDispImage(dataPtr, 0, 0);
	glColor3f(1.0, 0.0, 0.0);
	glLineWidth(6.0);

	/* detect the markers in the video frame */
	if (arDetectMarker(dataPtr, thresh,
		&marker_info, &marker_num) < 0) {
		cleanup();
		exit(0);
	}
	for (i = 0; i < marker_num; i++) {
		argDrawSquare(marker_info[i].vertex, 0, 0);
	}
	for (i = 0; i < marker_num; i++) {
	}
	
	/* check for known patterns */
	for (i = 0; i < objectnum; i++) {

		k = -1;
		for (j = 0; j < marker_num; j++) {
			if (object[i].id == marker_info[j].id) {
				/* you've found a pattern */
				//printf("Found pattern: %d ",patt_id);
				glColor3f(0.0, 1.0, 0.0);
				argDrawSquare(marker_info[j].vertex, 0, 0);
				if (k == -1) k = j;
				else /* make sure you have the best pattern (highest confidence factor) */
				if (marker_info[k].cf < marker_info[j].cf) k = j;
			}
		}
		if (k == -1) {
			object[i].visible = 0;
			//printf("object id = %d visibility - %d  \n", object[i].id, object[i].visible);
			continue;
		}

		/* calculate the transform for each marker */
		if (object[i].visible == 0) {
			arGetTransMat(&marker_info[k],
				object[i].marker_center, object[i].marker_width,
				object[i].trans);
		}
		else {
			arGetTransMatCont(&marker_info[k], object[i].trans,
				object[i].marker_center, object[i].marker_width,
				object[i].trans);
		}

		object[i].visible = 1;
	}

	///code here
	arVideoCapNext();
	/* draw the AR graphics */
	draw(object, objectnum);

	/*swap the graphics buffers*/
	argSwapBuffers();
}

static void init(void)
{
	udpInit();
	
	/*artoolkit init*/
	ARParam  wparam;
	/* open the video path */
	if (arVideoOpen(vconf) < 0) exit(0);
	/* find the size of the window */
	if (arVideoInqSize(&xsize, &ysize) < 0) exit(0);
	printf("Image size (x,y) = (%d,%d)\n", xsize, ysize);

	/* set the initial camera parameters */
	if (arParamLoad(cparam_name, 1, &wparam) < 0) {
		printf("Camera parameter load error !!\n");
		//exitdr(0);
	}
	arParamChangeSize(&wparam, xsize, ysize, &cparam);
	arInitCparam(&cparam);
	printf("*** Camera Parameter ***\n");
	arParamDisp(&cparam);

	/* load in the object data - trained markers and associated bitmap files */
	if ((object = read_ObjData(model_name, &objectnum)) == NULL) exit(0);
	printf("Objectfile num = %d\n", objectnum);
	for (int i = 0; i < objectnum; i++){
		printf("object id: %i __  name:  %s ___ \n", object[i].id, object[i].name);
	}

	/* open the graphics window */
	argInit(&cparam, 1.0, 0, 0, 0, 0);//baina
}
/* cleanup function called when program exits */
static void cleanup(void)
{
	closesocket(s);
	closesocket(s2);
	WSACleanup();
	arVideoCapStop();
	arVideoClose();
	argCleanup();
}



/* draw the the AR objects */
static int draw(ObjectData_T *object, int objectnum)
{
	int     i;
	double  gl_para[16];

	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_LIGHTING);

	/* calculate the viewing parameters - gl_para */

	int count = 0;
	
	float x1, x2,x3;
	float y1, y2,y3;
	float z1, z2,z3;


	for (i = 0; i < objectnum; i++) {
		if (object[i].visible == 0)continue;
		count++;
		argConvGlpara(object[i].trans, gl_para);
		draw_object(object[i].id, gl_para);
	}	


	x1 = object[0].trans[0][3], x2 = object[1].trans[0][3],x3 = object[2].trans[0][3];
	y1 = object[0].trans[1][3], y2 = object[1].trans[1][3], y3 = object[2].trans[1][3];
	z1 = object[0].trans[2][3], z2 = object[1].trans[2][3], z3 = object[2].trans[2][3];

	int grid = object[0].visible && object[1].visible;

	if (grid == 1){

		distX = fabs(x2 - x1);
		distY = fabs(y2 - y1);
		distZ = fabs(z2 - z1);

		//printf("grid distance %f %f %f \n ", distX, distY, distZ);
	}
	int controler =  object[2].visible;
	if (object[0].visible && controler){
		distX1 = fabs(x3 - x1);
		distY1 = fabs(y3 - y1);

		distX0 = fabs(x3 - x2);
		 distY0 = fabs(y3 - y2);
	}

	if (nFrame++ == 50) {
		nFrame = 0;

		if (controler){

			//calculate distance
			//printf("object id = %d ||  x = %f    || y =  %f     ||  z =  %f    ||\n", object[0].id, x1, y1, z1);
			//printf("object id = %d ||  x = %f    || y =  %f     ||  z =  %f    ||\n", object[1].id, x2, y2, z2);
			//printf("object id = %d ||  x = %f    || y =  %f     ||  z =  %f    ||\n", object[2].id, x3, y3, z3);

			printf(" distancia  0 - - 1    x = %f    || y =  %f     ||  z =  %f    ||\n", distX, distY, distZ);
			printf(" distancia  0-2 0-1 ||  x = %f  - - %f   || y =  %f - - %f       ||\n", distX1,distX0 ,  distY1, distY0);

			double x_send = distX1 / distX;
			double y_send = distY1 / distY;

			printf(" distancia norma   x = %f    || y =  %f    \n", x_send, y_send);
			
			/*
			// treat overflow
			if (x_send < 0.0)
				x_send = 0.0;
			else{
				if (y_send < 0.0)
					y_send = 0.0;
				else{
					if (x_send > 1.0)
						x_send = 1.0;
					else{
						if (y_send > 1.0)
							y_send = 1.0;
					}
				}
			}
			*/
			//printf(" distancia  normalizada   x = %f    || y =  %f    ||\n", x_send, y_send);

          	prepare_msg(x_send, y_send);

			udpSend(buf2);

			// if relevante change in distance then udpSend
		}
	}



	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	return(0);
}

/* draw the user object */
static int  draw_object(int obj_id, double gl_para[16])
{
	GLfloat   mat_ambient[] = { 0.0, 0.0, 1.0, 1.0 };
	GLfloat   mat_ambient_collide[] = { 1.0, 0.0, 0.0, 1.0 };
	GLfloat   mat_flash[] = { 0.0, 0.0, 1.0, 1.0 };
	GLfloat   mat_flash_collide[] = { 1.0, 0.0, 0.0, 1.0 };
	GLfloat   mat_flash_shiny[] = { 50.0 };
	GLfloat   light_position[] = { 100.0, -200.0, 200.0, 0.0 };
	GLfloat   ambi[] = { 0.1, 0.1, 0.1, 0.1 };
	GLfloat   lightZeroColor[] = { 0.9, 0.9, 0.9, 0.1 };

	argDrawMode3D();
	argDraw3dCamera(0, 0);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(gl_para);

	/* set the material */
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);

	glMaterialfv(GL_FRONT, GL_SHININESS, mat_flash_shiny);

	switch (obj_id){
	case 0:
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glColor3f(1.0, 2.0, 0.0);

		//glTranslated(10.0, 20.0, -100.0);
		draw_scene();
		//glTranslated(-10.0, -20.0, 100.0);
		break;
	case 1:
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glColor3f(1.0, 0.0, 0.0);
		//draw piano left
			break;
	case 2:
		
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash);
			glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
			glColor3f(1.0, 2.0, 0.0);
			draw_controller(distX/4, distY/4);
		break;
		}
		argDrawMode2D();

		return 0;
	}

/*
// Draw Module
*/
static int  draw_scene()
{


		draw_grid(distX , distY ); 

/*		glBegin(GL_QUADS);
		glVertex2d(distX, 0.0);
		glVertex2d(distX, -distY);
		glVertex2d(0.0, -distY);
		glVertex2d(0.0, 0.0);
		glEnd();
		*/

		glTranslated((distX/2)-250, -distY/2, 0.0);
		draw_signal();
		glTranslated(-(distX / 2)-250, +distY/2 , 0.0);
		//glEnd();
		/////
	argDrawMode2D();

	return 0;
}
static void draw_sin(double x_ , double y_){

	glTranslatef(x_, 0.0, 0.0);
	glutSolidTorus(10.0, 80.0, 8, 20);
	glTranslatef( -x_, 0.0, 0.0);


	for (int i = 0; i < 361; i++)
	{
		float x = (float)i;
		float y = x_ * sin(i  *(6.284 / 360.0));
		int f = 2;
		int temp = i - anime;
		if (abs(temp) <= 5){
			glTranslatef(x / f, y / f, 0.0);
			glutSolidSphere(1.2+0.6* temp, 8, 20);
			glTranslatef(-x / f, -y / f, 0.0);
		}
		else
		{
			glTranslatef(x / f, y / f, 0.0);
			glutSolidSphere(2.0, 8, 20);
			glTranslatef(-x / f, -y / f, 0.0);
		}
		
	}


}
static void draw_sqrt(void){
	glTranslatef(80.0, 0.0, 0.0);
	glutSolidTorus(10.0, 120.0, 8, 20);
	glTranslatef(-80.0, 0.0, 0.0);


	glBegin(GL_LINES);
	glVertex2d(0, 0);
	glVertex2d(40, 0);

	glVertex2d(40, 0);
	glVertex2d(40, 40);

	glVertex2d(40, 40);
	glVertex2d(80, 40);

	glVertex2d(80, 40);
	glVertex2d(80, 0);

	glVertex2d(80, 0);
	glVertex2d(120, 0);
	glEnd();
}
static void draw_saw(void){

	glTranslatef(80.0, 0.0, 0.0);
	glutSolidTorus(10.0, 120.0, 8, 20);
	glTranslatef(-80.0, 0.0, 0.0);

	// 
	glBegin(GL_POINTS);
	for (int i = 0; i<181; i++)
	{
		float a = 90.0;
		float x = (float)i;
		float y = 80.0 * (2 / a) * (x - a  * floor(x / a + 0.5)) *  pow((-1), floor(x / a + 0.5));
		glVertex2f(x, y);
	}
	glEnd();
}
static void draw_tri(void){

	glTranslatef(80.0, 0.0, 0.0);
	glutSolidTorus(10.0, 120.0, 8, 20);
	glTranslatef(-80.0, 0.0, 0.0);

	// 
	for (int i = 0; i<181; i++)
	{
		float a = 90.0;
		float x = (float)i;

		float current_t = (x / a - floor(0.5 + x / a));
		float x_ = x + 1;
		float next_t = (x_+1 / a - floor(0.5 + x_ / a));
		float y = 40.0 * 2 * current_t;
		//printf(" current_t %f \n next_t %f \n", current_t, next_t);
		if (abs(current_t) + next_t >= 0.4){

				glTranslatef(x, y, 0.0);
				glutSolidSphere(20.0, 8, 12);
				glTranslatef(-x, -y, 0.0);
			
		}
		else{
			glTranslatef(x, y, 0.0);
			glutSolidSphere(8.0, 8, 12);
			glTranslatef(-x, -y, 0.0);
		}
	}
}
static void draw_grid(double distX, double distY ){

	draw_line(0.0, 0.0 , 0.0, -distY);
	draw_line(0.0, 0.0, distX ,  0.0);
	draw_line(distX, -distY, distX, 0.0);
	draw_line(0.0, -distY, distX, -distY);
	
}

static void draw_grid3d(double distX, double distY,double distZ){


	draw_line3d(0.0, 0.0, 0.0, 0.0, -distY , -distZ);
	draw_line3d(0.0, 0.0, 0.0, distX, 0.0, -distZ);
	draw_line3d(distX, -distY, -distZ, distX, 0.0, -distZ);
	draw_line3d(0.0, -distY, -distZ, distX, -distY, -distZ);

}

static void draw_line3d(double xi, double yi,double zi, double xf, double yf,double zf){
	glBegin(GL_LINES);
	glVertex3d(xi, yi,zi);
	glVertex3d(xf,yf,zf);
	glEnd();
}
static void draw_line(double xi, double yi, double xf, double yf){

	glBegin(GL_LINES);
	glVertex2d(xi, yi);
	glVertex2d(xf, yf);
	glEnd();

}

static void draw_controller(double x, double y){

	glBegin(GL_QUADS);
	glVertex2d(x, 0.0);
	glVertex2d(x, -y);
	glVertex2d(0.0, -y);
	glVertex2d(0.0, 0.0);
	glEnd();
}
static void draw_signal(){

	glPointSize(0.5);
	for (int i = 0; i < 499; i++){
		draw_line((double)i, (double)sig[i] * 200, (double)(i + 1), (double)sig[i + 1] * 200);
	}
	


}



/*
/ UDP MODULE
*/

static void udpInit(){
	/*Init udp server*/
	//clear the buffer by filling null, it might have previously received data
	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	if ((s2 = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{

		printf("Could not create socket 2 : %d", WSAGetLastError());
	}
	printf("Socket 2 created.\n");
	//Create a socket
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
	}
	printf("Socket created.\n");

	//setup address structure
	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(PORT2);
	client.sin_addr.S_un.S_addr = inet_addr(SERVER);


	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);


	//Bind
	if (bind(s, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	puts("Bind done");
	iResult = ioctlsocket(s, FIONBIO, &iMode);
	if (iResult != NO_ERROR){
		printf("ioctlsocket failed with error: %ld\n", iResult);
	}
	puts("non blocking mode activated ");

}



static int udpReceive(){

	/*udp receive */
	//clear the buffer by filling null, it might have previously received data
	memset(buf, '\0', BUFLEN);
	//try to receive some data
	if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != 10035){
			printf("recvfrom() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}
	}
	else
		update();

	

	return 2;
}

static void udpSend(){
	

	printf("sending msg  %s \n ", buf2);
	//MSG_OOB
	if (sendto(s2, buf2, sizeof(buf2), 0, (struct sockaddr*) &client, clen) == SOCKET_ERROR)
	{
	if (WSAGetLastError() != 10047){
	printf("sendto() failed with error code : %d", WSAGetLastError());
	//exit(EXIT_FAILURE);
	}
	}
}

static int processMessage(void){
	//print details of the client/peer and the data received
	//printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
	//printf("Data: %s\n", buf);

	redraw = 1;

	return recv_len;
}

static void prepare_msg(double x_send, double y_send){


	char message[200]; 
	char str1[50];
	char str2[50];

	sprintf_s(message, 200, "%s", "setvalue"); //
	sprintf_s(str1, 50, "%.2f", x_send);
	sprintf_s(str2, 50, "%.2f", y_send);


	size_t len0 = strlen(message);
	size_t len1 = strlen(str1);
	size_t len2 = strlen(str2);


	char * s = malloc(len0 + len1 + len2 + 3);
	memcpy(s, message, len0);
	s[len0] = ' ';
	memcpy(s + len0 + 1, str1, len1 + 1);
	s[len0 + len1 + 1] = ' '; 
	memcpy(s + len0 + len1 + 2, str2, len2 + 1);
	s[len0 + len1 + len2  + 2] = ' ';
	s[len0 + len1 + len2 + 3] = '\0';
	memcpy(buf2, s, BUFLEN);
}
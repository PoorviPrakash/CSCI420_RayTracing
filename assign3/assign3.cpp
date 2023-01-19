/*
CSCI 420
Assignment 3 Raytracer

Name: Poorvi Burly Prakash
*/

#include <string.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#include "pic.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#define MAX_TRIANGLES 2000
#define MAX_SPHERES 10
#define MAX_LIGHTS 10

char *filename=0;

//different display modes
#define MODE_DISPLAY 1
#define MODE_JPEG 2
int mode=MODE_DISPLAY;

//you may want to make these smaller for debugging purposes
#define WIDTH 640
#define HEIGHT 480

//the field of view of the camera
#define fov 60.0

unsigned char buffer[HEIGHT][WIDTH][3];
float aspectRatio = (float) WIDTH/HEIGHT;
double origin[3]= {0.0,0.0,0.0};

double reflection[3];

struct Vertex
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double normal[3];
  double shininess;
};

Vertex vtl,vtr,vbl,vbr;
Vertex p1,p2,p3,p4;
Vertex vertices[HEIGHT][WIDTH];
float areas[3];
float totalArea=0.0;
double finalColor[3]={0.0,0.0,0.0};

typedef struct _Triangle
{
  struct Vertex v[3];
} Triangle;

typedef struct _Sphere
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double shininess;
  double radius;
} Sphere;

typedef struct _Light
{
  double position[3];
  double color[3];
} Light;

Triangle triangles[MAX_TRIANGLES];
Sphere spheres[MAX_SPHERES];
Light lights[MAX_LIGHTS];
double ambient_light[3];

int num_triangles=0;
int num_spheres=0;
int num_lights=0;

void plot_pixel_display(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel_jpeg(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel(int x,int y,unsigned char r,unsigned char g,unsigned char b);

//MATRIX OPERATIONS
void normalize(double *v){
  double mag = sqrt((v[0]*v[0])+(v[1]*v[1])+(v[2]*v[2]));
  v[0] = v[0]/mag;
  v[1] = v[1]/mag;
  v[2] = v[2]/mag;
} 

float dotProduct(double v1[3],double v2[3]){
  return ((v1[0]*v2[0])+(v1[1]*v2[1])+(v1[2]*v2[2]));
} 

void crossProduct(double v1[3],double v2[3], double *res)
{
    res[0] = v1[1] * v2[2] - v1[2] * v2[1];
    res[1] = v1[2] * v2[0] - v1[0] * v2[2];
    res[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

void getSphereNormal(double *normal,double p[3], float t,int idx)
{
    normal[0] = (p[0]*t) - spheres[idx].position[0];
    normal[1] = (p[1]*t) - spheres[idx].position[1];
    normal[2] = (p[2]*t) - spheres[idx].position[2];
    normalize(normal);
    
}

void getImageBorders(){
  double changeToRadians = 0.0174532925;
  //top-left : x = - a tan(fov/2), y = tan(fov/2), z=-1
  vtl.position[0] = (-aspectRatio)*tan(changeToRadians*fov/2);
  vtl.position[1] = tan(changeToRadians*fov/2);
  vtl.position[2] = -1.0;
  
  //bottom-left : x = - a tan(fov/2), y =  - tan(fov/2), z=-1
  vbl.position[0] = (-aspectRatio)*tan(changeToRadians*fov/2);
  vbl.position[1] = -tan(changeToRadians*fov/2);
  vbl.position[2] = -1.0;

  //top-right : x = a tan(fov/2), y = tan(fov/2), z=-1
  vtr.position[0] = (aspectRatio)*tan(changeToRadians*fov/2);
  vtr.position[1] = tan(changeToRadians*fov/2);
  vtr.position[2] = -1.0;

  //bottom-right : x = a tan(fov/2), y =  - tan(fov/2), z=-1
  vbr.position[0] = (aspectRatio)*tan(changeToRadians*fov/2);
  vbr.position[1] = -tan(changeToRadians*fov/2);
  vbr.position[2] = -1.0;

  //Setting values of every vertex on the image plane using subdivision
  for(int i=0;i<HEIGHT;i++)
    {
        vertices[i][0].position[0]=vtl.position[0];
        for(int j=0;j<WIDTH;j++)
        {
            vertices[i][j].position[2]=-1.0;
            if(j==0)
                vertices[i][j].position[0]=vbl.position[0];
            else
                vertices[i][j].position[0]=vertices[i][j-1].position[0]+((vtr.position[0]-vtl.position[0])/(WIDTH-1));
            if(i==0)
                vertices[i][j].position[1]=vbl.position[1];
            else
                vertices[i][j].position[1]=vertices[i-1][j].position[1]+((vtl.position[1]-vbl.position[1])/(HEIGHT-1));
        }
    }
}

float raySphereIntersection(double org[3], double direction[3], Sphere s)
{

    double o[3] = {org[0]-s.position[0],org[1]-s.position[1],org[2]-s.position[2]};
    
    float a = dotProduct(direction, direction);
    float b = 2 * dotProduct(direction, o);
    float c = dotProduct(o, o) - (s.radius * s.radius);
    
    float determinant = b * b - 4 * a * c;
    float distSqrt = sqrtf(determinant);

    if(determinant < 0.0) //Solution Exists only if sqrt(D) is Real (not Imaginary)
      return 0; 

    float sign = (c < -0.00001) ? 1 : -1;    //Ray Originates Inside Sphere If C < 0
    float t0 = (-b + sign*sqrtf(determinant))/(2*a); //Solve Quadratic Equation for Distance to Intersection
    float t1 = c / ((-b + sign*sqrtf(determinant))/2.0);
    if (t0 > t1)
    {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }
    if (t1 < 0)
        return 0;
    return (t0 < 0)? t1 : t0;
}

float rayTriangleIntersection(double org[3],double direction[3], Triangle *triangle)
{
  double edge1[3]={triangle->v[1].position[0]-triangle->v[0].position[0],triangle->v[1].position[1]-triangle->v[0].position[1],triangle->v[1].position[2]-triangle->v[0].position[2]};
    double edge2[3]={triangle->v[2].position[0]-triangle->v[0].position[0],triangle->v[2].position[1]-triangle->v[0].position[1],triangle->v[2].position[2]-triangle->v[0].position[2]};
    
    double prod[3];
    crossProduct(edge1,edge2,prod);
    
    
    double p[3];
    crossProduct(direction,edge2,p);
    
    normalize(prod);
    
    const float EPSILON = 0.0000001;
    float a = dotProduct(edge1, p);
    if (a > -EPSILON && a < EPSILON)
        return false;    // This ray is parallel to this triangle.

    float f =  1 / a;
    double s[3] ={org[0]-triangle->v[0].position[0],org[1]-triangle->v[0].position[1],org[2]-triangle->v[0].position[2]};
    float u = f*(dotProduct(s, p));
    if(u < 0.0 || u > 1.0)
        return 0;
    
    double q[3];
    crossProduct(s, edge1, q);
    float v = f * (dotProduct(direction, q));
    if(v < 0.0 || u + v > 1.0)
        return 0;
    
    // at this stage we can compute t to find out where
	// the intersection point is on the line
	float t0 = f * dotProduct(edge2,q);
    
	if (t0 > EPSILON) // ray intersection
		return t0;
    
	else // this means that there is a line intersection
        // but not a ray intersection
        return 0;   
}

void sphereShadowRays(Vertex *direction, double *l, float t, int idx){
  double v[3] = {direction->position[0]*t,direction->position[1]*t,direction->position[2]*t};
    bool flag=false;
    for(int i=0;i<num_spheres;i++)
    {
        //Check for intersection of the ray with another object. 
          if(i!=idx && raySphereIntersection(v, l, spheres[i]))
          {
            flag=true;
          }
    }
    for(int j=0;j<num_triangles;j++)
    {
        if(rayTriangleIntersection(v, l, &triangles[j]))
        {
            flag=true;
        }
    }
    
    
    if(flag){
        //Setting the color to black
        reflection[0] = 0;
        reflection[1] = 0;
        reflection[2] = 1;
        direction->color_diffuse[0] = 0;
        direction->color_diffuse[1] = 0;
        direction->color_diffuse[2] = 0;
        direction->color_specular[0] = 0;
        direction->color_specular[1] = 0;
        direction->color_specular[2] = 0;
    }
}

//SET COLOR FOR EACH SPHERE
void computeSphereColor(Vertex *direction,float t,int idx)
{
    //CALCULATING DIFFUSE COMPONENT - Lecture 5.1 slide 30
    //reversing the ray by multiplying it by -1
    double l[3]={-1*(direction->position[0]*t)+lights[0].position[0],-1*(direction->position[1]*t)+lights[0].position[1],-1*(direction->position[2]*t)+lights[0].position[2]};
    normalize(l);
    
    //l · n + clamping
    float lDotn = dotProduct(l, direction->normal);
    if(lDotn < 0.0)
      lDotn = 0.0;
    
    //Id = kdLd(l · n)
    direction->color_diffuse[0] = spheres[idx].color_diffuse[0]*lDotn;
    direction->color_diffuse[1] = spheres[idx].color_diffuse[1]*lDotn;
    direction->color_diffuse[2] = spheres[idx].color_diffuse[2]*lDotn;
    
    //CALCULATING SPECULAR COMPONENT - Lecture 5.1 slide 32
    double v[3] = {-1*direction->position[0]*t,-1*direction->position[1]*t,-1*direction->position[2]*t};
    
    //r = 2(l · n)n - l
    reflection[0] = (2*lDotn*direction->normal[0])-l[0];
    reflection[1] = (2*lDotn*direction->normal[1])-l[1];
    reflection[2] = (2*lDotn*direction->normal[2])-l[2]; 
    
    normalize(reflection);
    normalize(v);
    
    //v · r + clamping
    float rDotv = dotProduct(reflection, v);
    if (rDotv <0.0)
      rDotv = 0.0;
    
    //Is = ksLs(cos )^alpha
    direction->color_specular[0] = spheres[idx].color_specular[0]* pow(rDotv,spheres[idx].shininess);
    direction->color_specular[1] = spheres[idx].color_specular[1]* pow(rDotv,spheres[idx].shininess);
    direction->color_specular[2] = spheres[idx].color_specular[2]* pow(rDotv,spheres[idx].shininess);

    //Check if there is an object between the sphere and the light source. That is, there is a shadow
    sphereShadowRays(direction, l, t, idx);
}

void getTriAreas(int idx, int t, Vertex *ray){
    //area of triangle formed by ray, vertex 2 and vertex 3
    areas[0] = fabs(
                    ray->position[0]*t*(triangles[idx].v[1].position[1] - triangles[idx].v[2].position[1])
                    +triangles[idx].v[1].position[0]*(triangles[idx].v[2].position[1] - ray->position[1]*t)
                    +triangles[idx].v[2].position[0]*(ray->position[1]*t - triangles[idx].v[1].position[1])
                    )/2;
    
    //area of triangle formed by ray, vertex 2 and vertex 3
    areas[1] = fabs(
                    triangles[idx].v[0].position[0]*(ray->position[1]*t - triangles[idx].v[2].position[1])
                    +ray->position[0]*t*(triangles[idx].v[2].position[1] - triangles[idx].v[0].position[1])
                    +triangles[idx].v[2].position[0]*(triangles[idx].v[0].position[1] - ray->position[1]*t)
                    )/2;
    
    //area of triangle formed by ray, vertex 2 and vertex 3
    areas[2] = fabs(
                    triangles[idx].v[0].position[0]*(triangles[idx].v[1].position[1] - ray->position[1]*t)
                    +triangles[idx].v[1].position[0]*(ray->position[1]*t - triangles[idx].v[0].position[1])
                    +ray->position[0]*t*(triangles[idx].v[0].position[1] - triangles[idx].v[1].position[1])
                    )/2;

}

void triShadowRays(Vertex *direction, double *l, float t, int idx){
  double ray[3] = {direction->position[0]*t,direction->position[1]*t,direction->position[2]*t};
    bool flag=false;
    for(int i=0;i<num_spheres;i++)
    {
        if(raySphereIntersection(ray, l, spheres[i]))
        {
            flag=true;
        }
    }
    for(int j=0;j<num_triangles-1;j++)
    {
        if(j!=idx && rayTriangleIntersection(ray, l, &triangles[j]))
        {
            flag=true;
        }
    }
    if(flag){
        reflection[0] = 0;
        reflection[1] = 0;
        reflection[2] = 1;
        direction->color_diffuse[0] = 0;
        direction->color_diffuse[1] = 0;
        direction->color_diffuse[2] = 0;
        direction->color_specular[0] = 0;
        direction->color_specular[1] = 0;
        direction->color_specular[2] = 0;
    }
}

//SET COLOR FOR EACH TRAINGLE
void  computeTriangleColor(Vertex *direction,float t,int idx,int s)
{
    getTriAreas(idx,t,direction);
    totalArea = areas[0]+areas[1]+areas[2];
    float alpha, beta, gamma;
    alpha = areas[0]/totalArea;
    beta = areas[1]/totalArea;
    gamma = areas[2]/totalArea;

    ////CALCULATING NORMAL COMPONENT Ref- Lecture 8.2 Slide 22 
    //Barycentric Coordinates for triangle normals
    direction->normal[0] = alpha*triangles[idx].v[0].normal[0]+beta*triangles[idx].v[1].normal[0]+gamma*triangles[idx].v[2].normal[0];
    direction->normal[1] = alpha*triangles[idx].v[0].normal[1]+beta*triangles[idx].v[1].normal[1]+gamma*triangles[idx].v[2].normal[1];
    direction->normal[2] = alpha*triangles[idx].v[0].normal[2]+beta*triangles[idx].v[1].normal[2]+gamma*triangles[idx].v[2].normal[2];
    normalize(direction->normal);

    double light[3]={-direction->position[0]*t+lights[s].position[0],-direction->position[1]*t+lights[s].position[1],-direction->position[2]*t+lights[s].position[2]}; 
    normalize(light);
    
    float lDotn = dotProduct(light, direction->normal);
    if(lDotn < 0.0)
      lDotn = 0.0;
    
    double point1[3] = {triangles[idx].v[0].color_diffuse[0]*lDotn, triangles[idx].v[0].color_diffuse[1]*lDotn, triangles[idx].v[0].color_diffuse[2]*lDotn}; 
    
    double point2[3] = {triangles[idx].v[1].color_diffuse[0]*lDotn, triangles[idx].v[1].color_diffuse[1]*lDotn, triangles[idx].v[1].color_diffuse[2]*lDotn};
    
    double point3[3] = {triangles[idx].v[2].color_diffuse[0]*lDotn, triangles[idx].v[2].color_diffuse[1]*lDotn, triangles[idx].v[2].color_diffuse[2]*lDotn}; //blue

    //CALCULATING DIFFUSE COMPONENT 
    direction->color_diffuse[0] = alpha*point1[0]+beta*point2[0]+gamma*point3[0];
    direction->color_diffuse[1] = alpha*point1[1]+beta*point2[1]+gamma*point3[1];
    direction->color_diffuse[2] = alpha*point1[2]+beta*point2[2]+gamma*point3[2];
    
    
    //CALCULATING SPECULAR COMPONENT 
    double v[3] = {-direction->position[0]*t,-direction->position[1]*t,-direction->position[2]*t};
    
    reflection[0] = (2*lDotn*direction->normal[0])-light[0];
    reflection[1] = (2*lDotn*direction->normal[1])-light[1];
    reflection[2] = (2*lDotn*direction->normal[2])-light[2];
    normalize(reflection);
    normalize(v);

    float rDotv = dotProduct(reflection, v);
    if(rDotv<0.0)
      rDotv = 0.0;
    
    double point1Specular[3] = {triangles[idx].v[0].color_specular[0]* pow(rDotv,triangles[idx].v[0].shininess), triangles[idx].v[0].color_specular[1]* pow(rDotv,triangles[idx].v[0].shininess), triangles[idx].v[0].color_specular[2]* pow(rDotv,triangles[idx].v[0].shininess)};
    
    double point2Specular[3] = {triangles[idx].v[1].color_specular[0]* pow(rDotv,triangles[idx].v[1].shininess), triangles[idx].v[1].color_specular[1]* pow(rDotv,triangles[idx].v[1].shininess), triangles[idx].v[1].color_specular[2]* pow(rDotv,triangles[idx].v[1].shininess)};
    
    double point3Specular[3] = {triangles[idx].v[2].color_specular[0]* pow(rDotv,triangles[idx].v[2].shininess), triangles[idx].v[2].color_specular[1]* pow(rDotv,triangles[idx].v[2].shininess), triangles[idx].v[2].color_specular[2]* pow(rDotv,triangles[idx].v[2].shininess)}; 
    
    direction->color_specular[0] = alpha*point1Specular[0]+beta*point2Specular[0]+gamma*point3Specular[0];
    direction->color_specular[1] = alpha*point1Specular[1]+beta*point2Specular[1]+gamma*point3Specular[1];
    direction->color_specular[2] = alpha*point1Specular[2]+beta*point2Specular[2]+gamma*point3Specular[2];
    triShadowRays(direction,light,t,idx);
}


//MODIFY THIS FUNCTION
void draw_scene()
{
    getImageBorders();
    float tMin=-1;
    int kMin=-1;
    for(int i=0;i<HEIGHT;i++)
    {
        glPointSize(2.0);
        glBegin(GL_POINTS);
        for(int j=0;j<WIDTH;j++)
        {
            //normalizing the direction vector
            normalize(vertices[i][j].position);
            bool flag=false;
            float minDistance=100000;
            //Check if Ray intersects with any Triangle and get 1st point of intersection
            for(int k=0;k<num_triangles;k++)
            {
                float t = rayTriangleIntersection(origin,vertices[i][j].position, &triangles[k]);
                if(t!=0)
                {
                    float distance  = sqrtf( pow(vertices[i][j].position[0]*t-vertices[i][j].position[0],2)+pow(vertices[i][j].position[1]*t-vertices[i][j].position[1],2)+pow(vertices[i][j].position[2]*t-vertices[i][j].position[2],2));
                    if (distance<minDistance){
                        minDistance=distance;
                        tMin=t;
                        kMin=k;
                    }
                    flag=true;
                }
            }

            if(flag){
                finalColor[0]=0.0;
                finalColor[1]=0.0;
                finalColor[2]=0.0;
                
                for(int x=0;x<num_lights;x++){//summing all the color values
                    computeTriangleColor(&vertices[i][j],tMin,kMin,x);
                    finalColor[0]+=lights[x].color[0]*(vertices[i][j].color_diffuse[0]+vertices[i][j].color_specular[0]);
                    finalColor[1]+=lights[x].color[1]*(vertices[i][j].color_diffuse[1]+vertices[i][j].color_specular[1]);
                    finalColor[2]+=lights[x].color[2]*(vertices[i][j].color_diffuse[2]+vertices[i][j].color_specular[2]);
                }
                
                //Adding ambient color
                finalColor[0]+=ambient_light[0];
                finalColor[1]+=ambient_light[1];
                finalColor[2]+=ambient_light[2];
                
                if(finalColor[0]>1.0){
                  finalColor[0] = 1.0;
                }else if(finalColor[0]<0.0){
                  finalColor[0] = 0.0;
                }
                if(finalColor[1]>1.0){
                  finalColor[1] = 1.0;
                }else if(finalColor[1]<0.0){
                  finalColor[1] = 0.0;
                }
                if(finalColor[2]>1.0){
                  finalColor[2] = 1.0;
                }else if(finalColor[2]<0.0){
                  finalColor[2] = 0.0;
                }
                plot_pixel(j,i,finalColor[0]*255,finalColor[1]*255,finalColor[2]*255);
            }
            
            //Check if Ray intersects with Sphere
            for(int y=0;y<num_spheres;y++)
            {
                float t=raySphereIntersection(origin,vertices[i][j].position,spheres[y]);
                if(t!=0)
                {
                    getSphereNormal(vertices[i][j].normal,vertices[i][j].position,t,y);
                    computeSphereColor(&vertices[i][j],t,y);
                    
                    double finalColor[3]={lights[0].color[0]*ambient_light[0]+vertices[i][j].color_diffuse[0]+vertices[i][j].color_specular[0], lights[0].color[1]*ambient_light[1]+vertices[i][j].color_diffuse[1]+vertices[i][j].color_specular[1], lights[0].color[2]*ambient_light[2]+vertices[i][j].color_diffuse[2]+vertices[i][j].color_specular[2]};
                    
                    if(finalColor[0]>1.0){
                      finalColor[0] = 1.0;
                    }else if(finalColor[0]<0.0){
                      finalColor[0] = 0.0;
                    }
                    if(finalColor[1]>1.0){
                      finalColor[1] = 1.0;
                    }else if(finalColor[1]<0.0){
                      finalColor[1] = 0.0;
                    }
                    if(finalColor[2]>1.0){
                      finalColor[2] = 1.0;
                    }else if(finalColor[2]<0.0){
                      finalColor[2] = 0.0;
                    }
                    plot_pixel(j,i,finalColor[0]*255,finalColor[1]*255,finalColor[2]*255);
                }
            }
        }
        glEnd();
        glFlush();
    }
    printf("Done!\n"); fflush(stdout);
}

void plot_pixel_display(int x,int y,unsigned char r,unsigned char g,unsigned char b)
{
  glColor3f(((double)r)/256.f,((double)g)/256.f,((double)b)/256.f);
  glVertex2i(x,y);
}

void plot_pixel_jpeg(int x,int y,unsigned char r,unsigned char g,unsigned char b)
{
  buffer[HEIGHT-y-1][x][0]=r;
  buffer[HEIGHT-y-1][x][1]=g;
  buffer[HEIGHT-y-1][x][2]=b;
}

void plot_pixel(int x,int y,unsigned char r,unsigned char g, unsigned char b)
{
  plot_pixel_display(x,y,r,g,b);
  if(mode == MODE_JPEG)
      plot_pixel_jpeg(x,y,r,g,b);
}

void save_jpg()
{
  Pic *in = NULL;

  in = pic_alloc(640, 480, 3, NULL);
  printf("Saving JPEG file: %s\n", filename);

  memcpy(in->pix,buffer,3*WIDTH*HEIGHT);
  if (jpeg_write(filename, in))
    printf("File saved Successfully\n");
  else
    printf("Error in Saving\n");

  pic_free(in);      

}

void parse_check(char *expected,char *found)
{
  if(strcasecmp(expected,found))
    {
      char error[100];
      printf("Expected '%s ' found '%s '\n",expected,found);
      printf("Parse error, abnormal abortion\n");
      exit(0);
    }

}

void parse_doubles(FILE*file, char *check, double p[3])
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check(check,str);
  fscanf(file,"%lf %lf %lf",&p[0],&p[1],&p[2]);
  printf("%s %lf %lf %lf\n",check,p[0],p[1],p[2]);
}

void parse_rad(FILE*file,double *r)
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check("rad:",str);
  fscanf(file,"%lf",r);
  printf("rad: %f\n",*r);
}

void parse_shi(FILE*file,double *shi)
{
  char s[100];
  fscanf(file,"%s",s);
  parse_check("shi:",s);
  fscanf(file,"%lf",shi);
  printf("shi: %f\n",*shi);
}

int loadScene(char *argv)
{
  FILE *file = fopen(argv,"r");
  int number_of_objects;
  char type[50];
  int i;
  Triangle t;
  Sphere s;
  Light l;
  fscanf(file,"%i",&number_of_objects);

  printf("number of objects: %i\n",number_of_objects);
  char str[200];

  parse_doubles(file,"amb:",ambient_light);

  for(i=0;i < number_of_objects;i++)
    {
      fscanf(file,"%s\n",type);
      printf("%s\n",type);
      if(strcasecmp(type,"triangle")==0)
	{

	  printf("found triangle\n");
	  int j;

	  for(j=0;j < 3;j++)
	    {
	      parse_doubles(file,"pos:",t.v[j].position);
	      parse_doubles(file,"nor:",t.v[j].normal);
	      parse_doubles(file,"dif:",t.v[j].color_diffuse);
	      parse_doubles(file,"spe:",t.v[j].color_specular);
	      parse_shi(file,&t.v[j].shininess);
	    }

	  if(num_triangles == MAX_TRIANGLES)
	    {
	      printf("too many triangles, you should increase MAX_TRIANGLES!\n");
	      exit(0);
	    }
	  triangles[num_triangles++] = t;
	}
      else if(strcasecmp(type,"sphere")==0)
	{
	  printf("found sphere\n");

	  parse_doubles(file,"pos:",s.position);
	  parse_rad(file,&s.radius);
	  parse_doubles(file,"dif:",s.color_diffuse);
	  parse_doubles(file,"spe:",s.color_specular);
	  parse_shi(file,&s.shininess);

	  if(num_spheres == MAX_SPHERES)
	    {
	      printf("too many spheres, you should increase MAX_SPHERES!\n");
	      exit(0);
	    }
	  spheres[num_spheres++] = s;
	}
      else if(strcasecmp(type,"light")==0)
	{
	  printf("found light\n");
	  parse_doubles(file,"pos:",l.position);
	  parse_doubles(file,"col:",l.color);

	  if(num_lights == MAX_LIGHTS)
	    {
	      printf("too many lights, you should increase MAX_LIGHTS!\n");
	      exit(0);
	    }
	  lights[num_lights++] = l;
	}
      else
	{
	  printf("unknown type in scene description:\n%s\n",type);
	  exit(0);
	}
    }
  return 0;
}

void display()
{

}

void init()
{
  glMatrixMode(GL_PROJECTION);
  glOrtho(0,WIDTH,0,HEIGHT,1,-1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(1,1,1,1);
  glClear(GL_COLOR_BUFFER_BIT);
}

void idle()
{
  //hack to make it only draw once
  static int once=0;
  if(!once)
  {
      draw_scene();
      if(mode == MODE_JPEG)
	save_jpg();
    }
  once=1;
}

int main (int argc, char ** argv)
{
  if (argc<2 || argc > 3)
  {  
    printf ("usage: %s <scenefile> [jpegname]\n", argv[0]);
    exit(0);
  }
  if(argc == 3)
    {
      mode = MODE_JPEG;
      filename = argv[2];
    }
  else if(argc == 2)
    mode = MODE_DISPLAY;

  glutInit(&argc,argv);
  loadScene(argv[1]);

  glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
  glutInitWindowPosition(0,0);
  glutInitWindowSize(WIDTH,HEIGHT);
  int window = glutCreateWindow("Ray Tracer");
  glutDisplayFunc(display);
  glutIdleFunc(idle);
  init();
  glutMainLoop();
}

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

// Settings
unsigned int scrWidth = 800;
unsigned int scrHeight = 600;
const char* title = "Pong";
GLuint shaderProgram;

//Graphics Parameters
const float PADDLE_SPEED = 175.0f;
const float PADDLE_HEIGHT = 100.0f;
const float PADDLE_WIDTH = 10.0f;
const float HALF_PADDLE_HEIGHT = PADDLE_HEIGHT / 2.0f;
const float HALF_PADDLE_WIDTH = PADDLE_WIDTH / 2.0f;
const float BALL_DIAMETER = 16.0f;
const float BALL_RADIUS = BALL_DIAMETER / 2.0f;
const float PADDLE_OFFSET_BOUNDS = HALF_PADDLE_HEIGHT + BALL_RADIUS;

/* - 2D Vector Structure - */
struct vec2 {
	float x;
	float y;
};

//Public Offsets Arrays
vec2 paddleOffsets[2];
vec2 ballOffsets[1];

/* - Initialization Methods - */

// Initialize GLFW
void initGLFW(unsigned int versionMajor, unsigned int versionMinor) 
{
	glfwInit();

	//Pass in Window Parameters
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, versionMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, versionMinor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//MacOS specific Parameter
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
}

// Create Window
void createWindow(GLFWwindow*& window, const char* title, unsigned int width, unsigned int height, GLFWframebuffersizefun frameBufferSizeCallback) 
{
	window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (!window) {
		return;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, frameBufferSizeCallback);
}

//Load GLAD Library
bool loadGLAD() 
{
	return gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

/* - Shader Methods - */

// Read File
std::string readFile(const char* filename) 
{
	std::ifstream file;
	std::stringstream buf;
	std::string ret = "";

	// Open File
	file.open(filename);

	if (file.is_open()) {
		//Read Buffer
		buf << file.rdbuf();
		ret = buf.str();
	}
	else {
		std::cout << "Could not open " << filename << std::endl;
	}

	//Close File
	file.close();

	return ret;
}

//Generate Shader
int genShader(const char* filepath, GLenum type) 
{
	std::string shaderSrc = readFile(filepath);
	const GLchar* shader = shaderSrc.c_str();

	//Build and Compile Shader
	int shaderObj = glCreateShader(type);
	glShaderSource(shaderObj, 1, &shader, NULL);
	glCompileShader(shaderObj);

	//Check for Errors
	int success;
	char infoLog[512];
	glGetShaderiv(shaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shaderObj, 512, NULL, infoLog);
		std::cout << "Error in shader compilation: " << infoLog << std::endl;
		return -1;
	}

	return shaderObj;
}

//Generate Shader Program
int genShaderProgram(const char* vertexShaderPath, const char* fragmentShaderPath) 
{
	int shaderProgram = glCreateProgram();

	int vertexShader = genShader(vertexShaderPath, GL_VERTEX_SHADER);
	int fragmentShader = genShader(fragmentShaderPath, GL_FRAGMENT_SHADER);

	if (vertexShader == -1 || fragmentShader == -1) {
		return -1;
	}

	//Link Shaders
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	//Check for Errors
	int success;
	char infoLog[512];
	glGetShaderiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "Error in shader linking: " << infoLog << std::endl;
		return -1;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

//Bind Shader
void bindShader(int shaderProgram) 
{
	glUseProgram(shaderProgram);
}

//Set Projection
void setOrthographicProjection(int shaderProgram, float left, float right, float bottom, float top, float near, float far) 
{
	float mat[4][4] = {
		{ 2.0f / (right - left), 0.0f, 0.0f, 0.0f },
		{ 0.0f, 2.0f / (top - bottom), 0.0f, 0.0f },
		{ 0.0f, 0.0f, -2.0f / (far - near), 0.0f },
		{ -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0f }
	};

	//Bind Shader
	bindShader(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &mat[0][0]);
}

//Delete Shader
void deleteShader(int shaderProgram)
{
	glDeleteProgram(shaderProgram);
}

/* - Vertex Array Object/Buffer Object Methods - */

//Structure for VAO storing Array Object and it's Buffer Objects
struct VAO {
	GLuint val;
	GLuint posVBO;
	GLuint offsetVBO;
	GLuint sizeVBO;
	GLuint EBO;
};

//Generate VAO
void genVAO(VAO* vao) 
{
	glGenVertexArrays(1, &vao->val);
	glBindVertexArray(vao->val);
}

//Generate Buffer of Certain Type and Set Data
template<typename T>
void genBufferObject(GLuint& bo, GLenum type, GLuint noElements, T* data, GLenum usage) 
{
	glGenBuffers(1, &bo);
	glBindBuffer(type, bo);
	glBufferData(type, noElements * sizeof(T), data, usage);
}

//Update Data in Buffer Object
template<typename T>
void updateData(GLuint& bo, GLintptr offset, GLuint noElements, T*data) 
{
	glBindBuffer(GL_ARRAY_BUFFER, bo);
	glBufferSubData(GL_ARRAY_BUFFER, offset, noElements * sizeof(T), data);
}

//Set Attribute Pointers
template<typename T>
void setAttPointer(GLuint& bo, GLuint idx, GLuint size, GLenum type, GLuint stride, GLuint offset, GLuint divisor = 0) 
{
	glBindBuffer(GL_ARRAY_BUFFER, bo);
	glVertexAttribPointer(idx, size, type, GL_FALSE, stride * sizeof(T), (void*)(offset * sizeof(T)));
	glEnableVertexAttribArray(idx);
	if (divisor > 0) {
		//Reset idx attribute every divisor iteration through instances
		glVertexAttribDivisor(idx, divisor);
	}
}

//Draw VAO
void draw(VAO vao, GLenum mode, GLuint count, GLenum type, GLint indices, GLuint instanceCount = 1) 
{
	glBindVertexArray(vao.val);
	glDrawElementsInstanced(mode, count, type, (void*)indices, instanceCount);
}

//Unbind Buffer
void unbindBuffer(GLenum type) 
{
	glBindBuffer(type, 0);
}

//Unbind VAO
void unbindVAO() 
{
	glBindVertexArray(0);
}

//Deallocate VAO/VBO Memory
void cleanup(VAO vao) 
{
	glDeleteBuffers(1, &vao.posVBO);
	glDeleteBuffers(1, &vao.offsetVBO);
	glDeleteBuffers(1, &vao.sizeVBO);
	glDeleteBuffers(1, &vao.EBO);
	glDeleteVertexArrays(1, &vao.val);
}

//Generate arrays for Circle Model
void gen2DCircleArray(float*& vertices, unsigned int*& indices, unsigned int noTriangles, float radius = 0.5f) 
{
	vertices = new float[(noTriangles + 1) * 2];

	vertices[0] = 0.0f;
	vertices[1] = 0.0f;

	indices = new unsigned int[noTriangles * 3];

	float pi = 4 * atanf(1.0f);
	float noTrianglesF = (float)noTriangles;
	float theta = 0.0f;

	for (unsigned int i = 0; i < noTriangles; i++) {
		//Set Vertices
		vertices[(i + 1) * 2] = radius * cosf(theta);
		vertices[(i + 1) * 2 + 1] = radius * sinf(theta);
		
		//Set Indicies
		indices[i * 3 + 0] = 0;
		indices[i * 3 + 1] = i + 1;
		indices[i * 3 + 2] = i + 2;

		//Step up Theta
		theta += 2 * pi / noTriangles;
	}

	//Set Last Index to Wrap around to Begining
	indices[(noTriangles - 1) * 3 + 2] = 1;
}

/* - Main Loop Methods - */

// Callback for Window Size Change
void frameBufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	scrWidth = width;
	scrHeight = height;

	//Update Projection Matrix
	setOrthographicProjection(shaderProgram, 0, width, 0, height, 0.0f, 1.0f);
}

//Process Input
void processInput(GLFWwindow* window, double deltaTime, vec2 *paddleOffset) 
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}

	//Left Paddle
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		//Bounds
		if (paddleOffset[0].y < scrHeight - PADDLE_OFFSET_BOUNDS) { 
			paddleOffset[0].y += deltaTime * PADDLE_SPEED;
		}
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		//Bounds
		if (paddleOffset[0].y > PADDLE_OFFSET_BOUNDS) {
			paddleOffset[0].y -= deltaTime * PADDLE_SPEED;
		}	
	}

	//Right Paddle
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		//Bounds
		if (paddleOffset[1].y < scrHeight - PADDLE_OFFSET_BOUNDS) {
			paddleOffset[1].y += deltaTime * PADDLE_SPEED;
		}
	}

	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		//Bounds
		if (paddleOffset[1].y > PADDLE_OFFSET_BOUNDS) {
			paddleOffset[1].y -= deltaTime * PADDLE_SPEED;
		}
	}
}

//Clear Screen
void clearScreen() 
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

//New Frame
void newFrame(GLFWwindow* window) 
{
	glfwSwapBuffers(window);
	glfwPollEvents();
}

/* - Cleanup Methods - */

//Terminate GLFW
void cleanup() 
{
	glfwTerminate();
}

int main()
{
	std::cout << "Hello, Atari!" << std::endl;

	//Timing
	double deltaTime = 0.0;
	double lastFrame = 0.0;

	//Initialization
	initGLFW(3, 3);

	//Create Window
	GLFWwindow* window = nullptr;
	createWindow(window, title, scrWidth, scrHeight, frameBufferSizeCallback);
	if (!window) {
		std::cout << "Could not create window." << std::endl;
		cleanup();
		return -1;
	}

	//Load GLAD
	if (!loadGLAD()) {
		std::cout << "Could not initialize GLAD" << std::endl;
		cleanup();
		return -1;
	}

	glViewport(0, 0, (float)scrWidth, (float)scrHeight);

	//Shaders
	GLuint shaderProgram = genShaderProgram("main.vs", "main.fs");
	setOrthographicProjection(shaderProgram, 0, scrWidth, 0, scrHeight, 0.0f, 1.0f);

	/* - Paddle VAOs and VBOs - */

	//Setup Vertex data
	float paddleVertices[] = {
		0.5f, 0.5f,
		-0.5f, 0.5f,
		-0.5f, -0.5f,
		0.5f, -0.5f
	};

	//Setup Index Data
	unsigned int paddleIndices[] = {
		0, 1, 2, //top left trianlge
		2, 3, 0  //bottom right triangle
	};

	//Offsets Array
	paddleOffsets[0] = { 35.0f, scrHeight / 2.0f };
	paddleOffsets[1] = { scrWidth - 35.0f, scrHeight / 2.0f };

	//Size Array
	vec2 paddleSizes[] = {
		PADDLE_WIDTH, PADDLE_HEIGHT
	};

	//Setup VAO
	VAO paddleVAO;
	genVAO(&paddleVAO);

	//Position VBO
	genBufferObject<float>(paddleVAO.posVBO, GL_ARRAY_BUFFER, 2 * 4, paddleVertices, GL_STATIC_DRAW);
	setAttPointer<float>(paddleVAO.posVBO, 0, 2, GL_FLOAT, 2, 0);

	//Offset VBO
	genBufferObject<vec2>(paddleVAO.offsetVBO, GL_ARRAY_BUFFER, 2, paddleOffsets, GL_DYNAMIC_DRAW);
	setAttPointer<float>(paddleVAO.offsetVBO, 1, 2, GL_FLOAT, 2, 0, 1);

	//Size VBO
	genBufferObject<vec2>(paddleVAO.sizeVBO, GL_ARRAY_BUFFER, 1, paddleSizes, GL_STATIC_DRAW);
	setAttPointer<float>(paddleVAO.sizeVBO, 2, 2, GL_FLOAT, 2, 0, 2);

	//EBO
	genBufferObject<GLuint>(paddleVAO.EBO, GL_ELEMENT_ARRAY_BUFFER, 2 * 4, paddleIndices, GL_STATIC_DRAW);

	//Unbind VBO and VAO
	unbindBuffer(GL_ARRAY_BUFFER);
	unbindVAO();

	/* - Ball VAOs and VBOs - */

	float* ballVertices;
	unsigned int* ballIndices;
	unsigned int noTriangles = 50;
	gen2DCircleArray(ballVertices, ballIndices, noTriangles, 0.5f);

	//Offsets Array
	ballOffsets[0] = { scrWidth / 2.0f, scrHeight / 2.0f };

	//Size Array
	vec2 ballSizes[] = {
		BALL_DIAMETER, BALL_DIAMETER
	};

	//Setup VAO/VBOs
	VAO ballVAO;
	genVAO(&ballVAO);

	//Position VBO
	genBufferObject<float>(ballVAO.posVBO, GL_ARRAY_BUFFER, 2 * (noTriangles + 1), ballVertices, GL_STATIC_DRAW);
	setAttPointer<float>(ballVAO.posVBO, 0, 2, GL_FLOAT, 2, 0);
	
	//Offset VBO
	genBufferObject<vec2>(ballVAO.offsetVBO, GL_ARRAY_BUFFER, 1, ballOffsets, GL_DYNAMIC_DRAW);
	setAttPointer<float>(ballVAO.offsetVBO, 1, 2, GL_FLOAT, 2, 0, 1);

	//Size VBO
	genBufferObject<vec2>(ballVAO.sizeVBO, GL_ARRAY_BUFFER, 1, ballSizes, GL_STATIC_DRAW);
	setAttPointer<float>(ballVAO.sizeVBO, 2, 2, GL_FLOAT, 2, 0, 1);

	//EBO
	genBufferObject<unsigned int>(ballVAO.EBO, GL_ELEMENT_ARRAY_BUFFER, 3 * noTriangles, ballIndices, GL_STATIC_DRAW);

	//Unbind VBO and VAO
	unbindBuffer(GL_ARRAY_BUFFER);
	unbindVAO();

	//Render Loop
	while (!glfwWindowShouldClose(window)) 
	{
		//Update time
		deltaTime = glfwGetTime() - lastFrame;
		lastFrame += deltaTime;

		//Input
		processInput(window, deltaTime, paddleOffsets);

		//Clear screen for new frame
		clearScreen();

		//Update Data
		updateData<vec2>(paddleVAO.offsetVBO, 0, 2, paddleOffsets);
		updateData<vec2>(ballVAO.offsetVBO, 0, 1, ballOffsets);

		//Render Object
		bindShader(shaderProgram);
		draw(paddleVAO, GL_TRIANGLES, 3 * 2, GL_UNSIGNED_INT, 0, 2);
		draw(ballVAO, GL_TRIANGLES, 3 * noTriangles, GL_UNSIGNED_INT, 0);

		//Swap frames
		newFrame(window);
	}

	//Cleanup Memory
	cleanup(paddleVAO);
	cleanup(ballVAO);
	deleteShader(shaderProgram);
	cleanup();

	return 0;
}
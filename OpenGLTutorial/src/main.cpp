#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

// Settings
unsigned int scrWidth = 800;
unsigned int scrHeight = 600;
const char* title = "Pong Game";
GLuint shaderProgram;

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
		{ -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0f },
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
void processInput(GLFWwindow* window) 
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
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

	glViewport(0, 0, scrWidth, scrHeight);

	//Shaders
	GLuint shaderProgram = genShaderProgram("main.vs", "main.fs");
	setOrthographicProjection(shaderProgram, 0, scrWidth, 0, scrHeight, 0.0f, 1.0f);

	//Setup Vertex data
	float vertices[] = {
		//	 x		y
			 0.5f, 0.5f,
			-0.5f, 0.5f,
			-0.5f, -0.5f,
			 0.5f, -0.5f
	};

	//Setup Index Data
	unsigned int indices[] = {
		0, 1, 2, //top left trianlge
		2, 3, 0  //bottom right triangle
	};

	//Offsets Array
	float offsets[] = {
		200.0f, 200.0f
	};

	//Size Array
	float sizes[] = {
		50.0f, 50.0f
	};

	//Setup VAO/VBOs
	VAO vao;
	genVAO(&vao);

	//Position VBO
	genBufferObject<float>(vao.posVBO, GL_ARRAY_BUFFER, 2 * 4, vertices, GL_STATIC_DRAW);
	setAttPointer<float>(vao.posVBO, 0, 2, GL_FLOAT, 2, 0);
	
	//Offset VBO
	genBufferObject<float>(vao.offsetVBO, GL_ARRAY_BUFFER, 1 * 2, offsets, GL_DYNAMIC_DRAW);
	setAttPointer<float>(vao.offsetVBO, 1, 2, GL_FLOAT, 2, 0, 1);

	//Size VBO
	genBufferObject<float>(vao.sizeVBO, GL_ARRAY_BUFFER, 1 * 2, offsets, GL_DYNAMIC_DRAW);
	setAttPointer<float>(vao.sizeVBO, 2, 2, GL_FLOAT, 2, 0, 1);

	//EBO
	genBufferObject<unsigned int>(vao.EBO, GL_ELEMENT_ARRAY_BUFFER, 3 * 2, indices, GL_STATIC_DRAW);

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
		processInput(window);

		//Clear screen for new frame
		clearScreen();

		//Render Object
		bindShader(shaderProgram);
		draw(vao, GL_TRIANGLES, 3 * 2, GL_UNSIGNED_INT, 0);

		//Swap frames
		newFrame(window);
	}

	//Cleanup Memory
	cleanup(vao);
	deleteShader(shaderProgram);
	cleanup();

	return 0;
}
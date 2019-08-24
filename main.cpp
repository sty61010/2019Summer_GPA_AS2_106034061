#include "../Externals/Include/Include.h"

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3
#define PI 3.1415926
using namespace glm;
using namespace std;

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

int scene_seletion = 0;
float angle1 = 0.0, angle2 = 90.0;
float turn=0.0;
int now_position_x=0,now_position_y=0;

GLint um4p;
GLint um4mv;
GLint um4m;
GLint um4v;
GLint tex_location;
GLuint program;

mat4 view;                    // V of MVP, viewing matrix
mat4 projection;            // P of MVP, projection matrix
mat4 model= mat4(1.0f);                   // M of MVP, model matrix
mat4 offset_matrix1 = translate(mat4(), vec3(0.0f, -5.0f, 0.0f));
mat4 offset_matrix2 = translate(mat4(), vec3(0.0f, -50.0f, 0.0f));

vec3 eye_position = vec3(0.0f, 0.0f, 0.0f);
vec3 front_back = vec3(1.0, 0.0f, 0.0f);//
vec3 up_down = vec3(0.0f, 1.0f, 0.0f);
vec3 right_left_rotate;

//skybox
GLuint program_skybox;
GLuint skybox_vao;
GLuint skybox_tex;
GLint skybox_view;

char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}
// define a simple data structure for storing texture image raw data
typedef struct _TextureData
{
    _TextureData(void) :
    width(0),
    height(0),
    data(0)
    {
    }
    int width;
    int height;
    unsigned char* data;
} TextureData;

// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadPNG(const char* const pngFilepath)
{
    TextureData texture;
    int components;
    
    // load the texture with stb image, force RGBA (4 components required)
    stbi_uc *data = stbi_load(pngFilepath, &texture.width, &texture.height, &components, 4);
    
    // is the image successfully loaded?
    if (data != NULL)
    {
        // copy the raw data
        size_t dataSize = texture.width * texture.height * 4 * sizeof(unsigned char);
        texture.data = new unsigned char[dataSize];
        memcpy(texture.data, data, dataSize);
        
        // mirror the image vertically to comply with OpenGL convention
        for (size_t i = 0; i < texture.width; ++i)
        {
            for (size_t j = 0; j < texture.height / 2; ++j)
            {
                for (size_t k = 0; k < 4; ++k)
                {
                    size_t coord1 = (j * texture.width + i) * 4 + k;
                    size_t coord2 = ((texture.height - j - 1) * texture.width + i) * 4 + k;
                    std::swap(texture.data[coord1], texture.data[coord2]);
                }
            }
        }
        // release the loaded image
        stbi_image_free(data);
    }
    return texture;
}

struct Material
{
    GLuint diffuse_tex;
};
struct Vertex{
    glm::vec3 Position;
    glm::vec2 TexCoords;
    glm::vec3 Normal;
};
struct Texture{
    GLuint id;
    string type;
    aiString path;
};

class Mesh {
public:
    Mesh(vector<Vertex> vertices, vector<GLuint> indices, vector<Texture> textures, GLuint ID, int count){
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        
        this->setupMesh();
        this->materialID=ID;
        this->drawCount=count;
    }
    void setupMesh(){
        glGenVertexArrays(1, &this->vao);
        glGenBuffers(1, &this->vbo);
        glGenBuffers(1, &this->ibo);
        
        glBindVertexArray(this->vao);
        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(Vertex), &this->vertices[0], GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(GLuint), &this->indices[0], GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)offsetof(Vertex, TexCoords));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)offsetof(Vertex, Normal));
        glBindVertexArray(0);
        
    }
    // Render the mesh
    void Draw(){
        glBindVertexArray(this->vao);
        glBindTexture(GL_TEXTURE_2D, this->materialID);
        glDrawElements(GL_TRIANGLES, this->drawCount, GL_UNSIGNED_INT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
private:
    GLuint vao, ibo, vbo;
    vector<Vertex> vertices;
    vector<GLuint> indices;
    vector<Texture> textures;
    int drawCount;
    GLuint materialID;

};
class Model{
public:
    Model(){
    }
    void loadModel(const char * path){
        const aiScene *scene = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);
        std::string path_str(path);
        this->directory = path_str.substr(0, path_str.find_last_of('/'));
        
        for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
            aiMaterial *source_material = scene->mMaterials[i];
            struct Material material;
            TextureData texture_data;
            aiString texturePath;
            if (source_material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS) {
                cout << "load texture image " << this->directory + "/" + (std::string)texturePath.C_Str() << endl;
                //texture_data[i] = loadPNG((char *)(this->directory + "/" + (std::string)texturePath.C_Str()).data());
                texture_data = loadPNG((char *)(this->directory + "/" + (std::string)texturePath.C_Str()).data());
            }
            else {
                cout << "load default image" << endl;
                //texture_data[i] = loadPNG((char *)"../Assets/default.png");
                texture_data = loadPNG((char *)"../Assets/default.png");
            }
            
            glGenTextures(1, &material.diffuse_tex);
            glBindTexture(GL_TEXTURE_2D, material.diffuse_tex);
            //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_data[i].width, texture_data[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data[i].data);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_data.width, texture_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data.data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            delete [] texture_data.data;
            // save material
            this->materials.push_back(material);
        }
        for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
            cout << this->materials[i].diffuse_tex << endl;
        }
        this->processNode(scene->mRootNode, scene);
    }
    
    void processNode(aiNode* node, const aiScene* scene){
        for (GLuint i = 0; i < node->mNumMeshes; i++){
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            this->meshes.push_back(this->processMesh(mesh, scene));
        }
        for (GLuint i = 0; i < node->mNumChildren; i++){
            this->processNode(node->mChildren[i], scene);
        }
    }
    
    Mesh processMesh(aiMesh *mesh, const aiScene *scene){
        vector<Vertex> vertices;
        vector<GLuint> indices;
        vector<Texture> textures;
        for (GLuint i = 0; i < mesh->mNumVertices; i++){
            Vertex vertex;
            glm::vec3 vector;
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
            
            if (mesh->mTextureCoords[0]) {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else{
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }
        for (GLuint i = 0; i < mesh->mNumFaces; i++){
            aiFace face = mesh->mFaces[i];
            for (GLuint j = 0; j < face.mNumIndices; j++){
                indices.push_back(face.mIndices[j]);
            }
        }
        // this->tex_map.push_back(this->mat[mesh->mMaterialIndex].diffuse_tex);
        return Mesh(vertices, indices, textures, this->materials[mesh->mMaterialIndex].diffuse_tex, mesh->mNumFaces*3);
    }
    void Draw(){
        for (GLuint i = 0; i < this->meshes.size(); i++){
            //glBindTexture(GL_TEXTURE_2D, this->mat[this->tex_map[i]].diffuse_tex);
            //glBindTexture(GL_TEXTURE_2D, this->tex_map[i]);
            this->meshes[i].Draw();
        }
    }
private:
    vector<Mesh> meshes;
    string directory;
    vector<struct Material> materials;
    vector<Texture> textures_loaded;
    vector<GLuint> tex_map;
};
Model model_1, model_2;

void My_Init()
{
    glClearColor(1.0f, 1.0f, 0.8f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
    // ---------------------------------------
    GLint GlewInitResult = glewInit();
    if (GLEW_OK != GlewInitResult)
    {
        printf("ERROR: %s",glewGetErrorString(GlewInitResult));
        exit(EXIT_FAILURE);
    }
    program_skybox = glCreateProgram();
    char** skybox_vs_glsl = loadShaderSource("../Assets/skybox_vs.vs.glsl");
    char** skybox_fs_glsl = loadShaderSource("../Assets/skybox_fs.fs.glsl");
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, skybox_vs_glsl, NULL);
    glShaderSource(fs, 1, skybox_fs_glsl, NULL);
    glCompileShader(vs);
    glCompileShader(fs);
    glAttachShader(program_skybox, vs);
    glAttachShader(program_skybox, fs);
    glLinkProgram(program_skybox);
    glUseProgram(program_skybox);
    
    skybox_view = glGetUniformLocation(program_skybox, "view");
    TextureData front = loadPNG("../Assets/mp_hexagon/hexagon_ft.tga");
    TextureData back = loadPNG("../Assets/mp_hexagon/hexagon_bk.tga");
    TextureData left = loadPNG("../Assets/mp_hexagon/hexagon_lf.tga");
    TextureData right = loadPNG("../Assets/mp_hexagon/hexagon_rt.tga");
    TextureData up = loadPNG("../Assets/mp_hexagon/hexagon_up.tga");
    TextureData down = loadPNG("../Assets/mp_hexagon/hexagon_dn.tga");

    glGenTextures(1, &skybox_tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 0, 0, GL_RGBA, front.width, front.height , 0, GL_RGBA, GL_UNSIGNED_BYTE, front.data);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 1, 0, GL_RGBA, back.width, back.height , 0, GL_RGBA, GL_UNSIGNED_BYTE, back.data);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 2, 0, GL_RGBA, up.width, up.height , 0, GL_RGBA, GL_UNSIGNED_BYTE, up.data);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 3, 0, GL_RGBA, down.width, down.height , 0, GL_RGBA, GL_UNSIGNED_BYTE, down.data);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 4, 0, GL_RGBA, left.width, left.height , 0, GL_RGBA, GL_UNSIGNED_BYTE, left.data);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 5, 0, GL_RGBA, right.width, right.height , 0, GL_RGBA, GL_UNSIGNED_BYTE, right.data);
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glGenVertexArrays(1, &skybox_vao);
    // ---------------------------------------
//    GLint GlewInitResult = glewInit();
    if (GLEW_OK != GlewInitResult){
        printf("ERROR: %s",glewGetErrorString(GlewInitResult));
        exit(EXIT_FAILURE);
    }
    program = glCreateProgram();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");
    char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
    freeShaderSource(vertexShaderSource);
    freeShaderSource(fragmentShaderSource);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    shaderLog(vertexShader);
    shaderLog(fragmentShader);
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    um4p = glGetUniformLocation(program, "um4p");
    um4mv = glGetUniformLocation(program, "um4mv");
    tex_location = glGetUniformLocation(program, "tex");
    glUseProgram(program);
    
    model_1.loadModel("../Assets/rungholt/house.obj");
    model_2.loadModel("../Assets/lost_empire/lost_empire.obj");
}


void My_Display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (scene_seletion == 0)
        model = offset_matrix1;
    else
        model = offset_matrix2;
    view = lookAt(eye_position, eye_position + front_back,up_down );
    // --------------------------------------------------------------
    static const GLfloat gray[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    static const GLfloat ones[] = { 1.0f };
    mat4 inv_vp_matrix = inverse(projection * view);
    
    glClearBufferfv(GL_COLOR, 0, gray);
    glClearBufferfv(GL_DEPTH, 0, ones);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);
    glUseProgram(program_skybox);
    glBindVertexArray(skybox_vao);
    
    vec3 view_vec_in = front_back;
    view_vec_in.y = -view_vec_in.y;
    view_vec_in.x = -view_vec_in.x;
    mat4 view_inv_mat = lookAt(eye_position, eye_position + view_vec_in, up_down);
    glUniformMatrix4fv(skybox_view, 1, GL_FALSE, &view_inv_mat[0][0]);
    
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glEnable(GL_DEPTH_TEST);
    // --------------------------------------------------------------
    glUseProgram(program);
    glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * model));
    glUniformMatrix4fv(um4m, 1, GL_FALSE, value_ptr(model));
    glUniformMatrix4fv(um4v, 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));
    glUniform1i(tex_location, 0);
    if (scene_seletion == 0)
        model_1.Draw();
    else
        model_2.Draw();
    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
    glViewport(0, 0, width, height);
    float viewportAspect = (float)width / (float)height;
    projection = perspective(radians(60.0f), viewportAspect, 0.1f, 1000.0f);
    view = lookAt(vec3(7.5f, 5.0f, 7.5f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
}

void My_Timer(int val)
{
    timer_cnt++;
    glutPostRedisplay();
    if(timer_enabled)
    {
        glutTimerFunc(timer_speed, My_Timer, val);
    }
}

void My_Mouse(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON)
    {
        if (state == GLUT_DOWN)
        {
            printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
            now_position_x=x;
            now_position_y=y;
        }
        else if (state == GLUT_UP)
        {
            printf("Mouse %d is released at (%d, %d)\n", button, x, y);
        }
    }
}
void Eye_Direction(int dx, int dy) {
    printf("moving (%d, %d)\n", dx, dy);
    //polar coordinate
    angle1 -= (float)dx/10;//horizontal-axis
    angle2 -= (float)dy/10;//vertical-axis
    if (angle1 > 360.0)
        angle1 -= 360.0;
    else if (angle1 < 0)
        angle1 += 360;
    
    if (angle2 > 180.0)
        angle2 = 179.9;
    else if (angle2 < 0)
        angle2 = 0.1;
    
    front_back.x = std::sinf(angle2 / 180.0 * PI) * std::cosf(angle1/ 180.0 * PI);
    front_back.z = std::sinf(angle2 / 180.0 * PI) * std::sinf(angle1 / 180.0 * PI);
    front_back.y = std::cosf(angle2 / 180.0 * PI);
    right_left_rotate = cross(front_back, up_down);
}
void My_Keyboard(unsigned char key, int x, int y){
    printf("Key %c is pressed at (%d, %d)\n", key, x, y);
    
    switch (key){
        case 's':
            eye_position -= front_back;
            /////////////////////
//            eye_position.x-=front_back.x;
//            eye_position.z-=front_back.z;
            break;
        case 'w':
            eye_position += front_back;
            /////////////////////
//            eye_position.x+=front_back.x;
//            eye_position.z+=front_back.z;
            break;
        case 'a':
//            eye_position -= right_left_rotate;
            ///////////////////////
            turn+=0.01f;
            front_back.x=sin(turn);
            front_back.z=cos(turn);
            break;
        case 'd':
//            eye_position += right_left_rotate;
            ///////////////////////
            turn-=0.01f;
            front_back.x=sin(turn);
            front_back.z=cos(turn);
            break;
        case 'z':
            eye_position += up_down;
            break;
        case 'x':
            eye_position -= up_down;
            break;
        case 'c':
            if (scene_seletion == 0)
                scene_seletion= 1;
            else
                scene_seletion = 0;
            break;
    }
}

void My_SpecialKeys(int key, int x, int y)
{
    switch(key)
    {
        case GLUT_KEY_F1:
            printf("F1 is pressed at (%d, %d)\n", x, y);
            break;
        case GLUT_KEY_PAGE_UP:
            printf("Page up is pressed at (%d, %d)\n", x, y);
            break;
        case GLUT_KEY_LEFT:
            printf("Left arrow is pressed at (%d, %d)\n", x, y);
            break;
        case GLUT_KEY_RIGHT:
            printf("Right arrow is pressed at (%d, %d)\n", x, y);
            break;
        case GLUT_KEY_UP:
            printf("Up arrow is pressed at (%d, %d)\n", x, y);
            break;
        case GLUT_KEY_DOWN:
            printf("Down arrow is pressed at (%d, %d)\n", x, y);
            break;
        default:
            printf("Other special key is pressed at (%d, %d)\n", x, y);
            break;
    }
    //    glutPostRedisplay();
}
void My_Menu(int id)
{
	switch(id)
	{
	case MENU_TIMER_START:
		if(!timer_enabled)
		{
			timer_enabled = true;
			glutTimerFunc(timer_speed, My_Timer, 0);
		}
		break;
	case MENU_TIMER_STOP:
		timer_enabled = false;
		break;
	case MENU_EXIT:
		exit(0);
		break;
	default:
		break;
	}
}
void Move_Mouse(int x,int y){
    cout<<"Mouse Moving:x->"<<x<<", y->"<<y<<endl;
    Eye_Direction(x-now_position_x, y-now_position_y);
    
    now_position_x=x;
    now_position_y=y;
}
int main(int argc, char *argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(1200, 900);
	glutCreateWindow("106034061_AS2");
#ifdef _MSC_VER
	glewInit();
#endif
    glPrintContextInfo();
	My_Init();

	int menu_main = glutCreateMenu(My_Menu);
    int menu_timer = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddSubMenu("Timer", menu_timer);
	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_timer);
	glutAddMenuEntry("Start", MENU_TIMER_START);
	glutAddMenuEntry("Stop", MENU_TIMER_STOP);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0); 
    glutMotionFunc(Move_Mouse);

    glutMainLoop();
	return 0;
}

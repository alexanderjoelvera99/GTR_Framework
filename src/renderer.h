#pragma once
#include "prefab.h"
#include "fbo.h"
#include "renderCall.h"

//forward declarations
class Camera;

namespace GTR {

	class Prefab;
	class Material;
	
	enum eMultipleLightRendering{
		SINGLEPASS = 0,
		MULTIPASS = 1,
        NOMULTIPLELIGHT = 2
	};
	
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
		std::vector<RenderCall*> render_call_vector;
		eMultipleLightRendering multiple_light_rendering;
		std::string shader_name;

	public:
        Renderer(GTR::eMultipleLightRendering multiple_light_rendering, std::string shader_name);
        
		// Change multiple light rendering type
		void changeMultiLightRendering();
        
        // Singlepass rendering function
        void singlepassRendering(std::vector<LightEntity*> light_entities, Shader* shader, Mesh* mesh);
        
        // Multipass rendering function
		void multipassRendering(std::vector<LightEntity*> lights, Shader* shader, Mesh* mesh, Material* material);
        
        //renders several elements of the scene
        void renderScene(GTR::Scene* scene, Camera* camera);

		//Sort render call
		void collectRenderCall(GTR::Scene* scene, Camera* camera, std::vector<RenderCall*>* rc_vector);
        
        // Clear render_call_vector
		void clearRenderCall(std::vector<RenderCall*>* rc_vector);
        
        // Render to texture function
        void renderToTexture(Camera* camera, FBO* fbo, std::vector<RenderCall*> rc_vector);

        // Rendering light depth buffer for shadow maps
        void renderLightDepthBuffer(LightEntity* light, std::vector<RenderCall*> rc_vector);

        // View the depth buffer
        void viewDepthBuffer(LightEntity* light);
	
		//to render a whole prefab (with all its nodes)
		void collectPrefabInRenderCall(const Matrix44& model, GTR::Prefab* prefab, Camera* camera, std::vector<RenderCall*>* rc_vector);

		//to render one node from the prefab and its children
		void collectNodesInRenderCall(const Matrix44& model, GTR::Node* node, Camera* camera, std::vector<RenderCall*>* rc_vector);
        
        // Render only the mesh for depth buffer texture
        void renderMesh(const Matrix44 model, Mesh* mesh, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};

	Texture* CubemapFromHDRE(const char* filename);

};

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
        
        // Multipass rendering function
		void multipassRendering(std::vector<LightEntity*> lights, Shader* shader, Mesh* mesh);
        
        //renders several elements of the scene
        void renderScene(GTR::Scene* scene, Camera* camera);

		//Sort render call
		void collectRenderCall(GTR::Scene* scene, Camera* camera);
        
        // Clear render_call_vector
		void clearRenderCall();
        
        // Render to texture function
        void renderToTexture(Scene* scene, Camera* camera, FBO* fbo);

        // Rendering light depth buffer for shadow maps
        void renderLightDepthBuffer(Scene* scene, LightEntity* light);

        // View the depth buffer
        void viewDepthBuffer(LightEntity* light);
	
		//to render a whole prefab (with all its nodes)
		void collectPrefabInRenderCall(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void collectNodesInRenderCall(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};

	Texture* CubemapFromHDRE(const char* filename);

};

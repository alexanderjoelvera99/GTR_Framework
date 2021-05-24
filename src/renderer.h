#pragma once
#include "prefab.h"
#include "fbo.h"


//forward declarations
class Camera;

namespace GTR {

	class Prefab;
	class Material;
	
	enum eMultipleLightRendering{
		SINGLEPASS = 0,
		MULTIPASS = 1,
        NOMULTIPLELIGHT = 2 //shadow??
	};
	
    enum ePipelineMode {
        FORWARD,
        DEFERRED
    };

    enum eRenderMode {
        SHOW_TEXTURE,
        SHOW_NORMAL,
        SHOW_OC,
        SHOW_UVS,
        SINGLE,
        MULTI,
        GBUFFERS

    };
    class RenderCall
    {
    public:
        Matrix44 model;
        Mesh* mesh;
        Material* material;
        float distance_to_camera;

        //ctor
        RenderCall()
        {
//            this->model = NULL;
//            this->mesh = NULL;
//            this->material = NULL;
//            this->distance_to_camera = NULL;
        }

        //~RenderCall(); 

        // sorting function
        
        /*
        static bool sorting_renderCalls(const RenderCall* rc_a, const RenderCall* rc_b) {
            if (rc_a->material->alpha_mode < rc_b->material->alpha_mode)
                return true;
            return false;
        }
        */
        
    };


	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
    public:

		std::vector< RenderCall> render_call_vector;

		eMultipleLightRendering multiple_light_rendering;
		std::string shader_name;

        eRenderMode render_mode;
        ePipelineMode pipeline_mode;

        Texture* color_buffer;


        FBO fbo;
        FBO gbuffers_fbo;
        bool show_gbuffers;

	public:
        // The light number that is selected to control with light controls
        int selected_light;
        
        
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
		void collectRenderCall(GTR::Scene* scene, Camera* camera, std::vector<RenderCall>& rc ); //V= &render_call_vector
        
        // Clear render_call_vector
		void clearRenderCall(std::vector<RenderCall*>* rc_vector);
	
        void renderForward(GTR::Scene* scene, std::vector <RenderCall>& rendercalls, Camera* camera);

        void renderDeferred(GTR::Scene* scene, std::vector<RenderCall>& rendercalls, Camera* camera);

		//to render a whole prefab (with all its nodes)
		void collectPrefabInRenderCall(const Matrix44& model, GTR::Prefab* prefab, Camera* camera, std::vector<RenderCall>& rc);

		//to render one node from the prefab and its children
		void collectNodesInRenderCall(const Matrix44& model, GTR::Node* node, Camera* camera, std::vector<RenderCall>& rc);
        
        // Render to texture function
        void renderToTexture(Camera* camera, FBO* fbo, std::vector<RenderCall>& rc_vector);

        // Rendering light depth buffer for shadow maps
        void renderLightDepthBuffer(LightEntity* light, std::vector<RenderCall>& rc_vector);

        // View the depth buffer
        void viewDepthBuffer(LightEntity* light);
        
        // Render only the mesh for depth buffer texture
        void renderMesh(const Matrix44 model, Mesh* mesh, Camera* camera, eAlphaMode material_alpha_mode);

		//to render one mesh given its material and transformation matrix
        void renderMeshWithMaterial(eRenderMode mode, const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
    };

	Texture* CubemapFromHDRE(const char* filename);

};

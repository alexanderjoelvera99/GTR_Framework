#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "scene.h"
#include "extra/hdre.h"
#include "application.h"


using namespace GTR;
Renderer::Renderer(GTR::eMultipleLightRendering multiple_light_rendering, std::string shader_name){
	this->multiple_light_rendering = multiple_light_rendering;
	this->shader_name = shader_name;
    this->selected_light = 0;
}

void Renderer::changeMultiLightRendering(){
	this->multiple_light_rendering = static_cast<GTR::eMultipleLightRendering>((this->multiple_light_rendering + 1) % 3);
	if(multiple_light_rendering == SINGLEPASS){
		shader_name = "singlepass";
	}
	else if(multiple_light_rendering == MULTIPASS || multiple_light_rendering == NOMULTIPLELIGHT){
		shader_name = "light";
	}
}

void Renderer::singlepassRendering(std::vector<LightEntity*> light_entities, Shader* shader, Mesh* mesh)
{
    int number_of_lights = (int)light_entities.size();
    Vector3 light_color[5];
    Vector3 light_position[5];
    eLightType light_type[5];
    Vector3 target[5];
    float max_distance[5];
    float cone_angle[5];

    for (int i = 0; i < number_of_lights; i++){
        GTR::LightEntity* light = light_entities[i];
        
        light_color[i] = light->color;
        light_position[i] = light->model.getTranslation();
        light_type[i] = light->light_type;
        target[i] = light->model.frontVector();;
        max_distance[i] = light->max_distance;
        cone_angle[i] = light->cone_angle;
    }
    shader->setUniform3Array("u_light_color",(float*)&light_color, number_of_lights);
    shader->setUniform3Array("u_light_position",(float*)&light_position, number_of_lights);
    shader->setUniform1Array("u_light_type",(int*)&light_type, number_of_lights);
    shader->setUniform3Array("u_light_direction",(float*)&target, number_of_lights);
    shader->setUniform1Array("u_max_distance",(float*)&max_distance, number_of_lights);
    shader->setUniform1Array("u_cone_angle",(float*)&cone_angle, number_of_lights);
    shader->setUniform1("u_num_lights", number_of_lights);

    //do the draw call that renders the mesh into the screen
    mesh->render(GL_TRIANGLES);
}
void Renderer::multipassRendering(std::vector<LightEntity*> lights, Shader* shader, Mesh* mesh, Material* material){
    int num_lights = (int)lights.size();
    
    //allow to render pixels that have the same depth as the one in the depth buffer
    glDepthFunc( GL_LEQUAL );

    //set blending mode to additive
    //this will collide with materials with blend...
    glBlendFunc( GL_SRC_ALPHA,GL_ONE );
    
    for(int i = 0; i < num_lights; ++i)
    {
        //first pass doesn't use blending
        if(i == 0 )
            glDisable( GL_BLEND );
        
        else{
            glEnable( GL_BLEND );
            shader->setUniform("u_emissive_factor", Vector3(0,0,0));
            shader->setUniform("u_ambient_light", Vector3(0,0,0));
        }
        
        if(material->alpha_mode == GTR::eAlphaMode::BLEND){
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
        }

        //pass the light data to the shader
        lights[i]->setUniforms( shader );

        //render the mesh
        mesh->render(GL_TRIANGLES);
    }

    glDisable( GL_BLEND );
    glDepthFunc( GL_LESS ); //as default
}

void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
    // Collecting render calls
    collectRenderCall(scene, camera, &this->render_call_vector);
    // sorting by alpha
    std::sort(render_call_vector.begin(), render_call_vector.end(), RenderCall::sorting_renderCalls);
    
    // Render to depth buffer of every light to create Shadow Maps
    std::vector<GTR::LightEntity*> lights = scene->light_entities;
    
    for (int i = 0; i < lights.size(); i++){
        // Collecting render calls for every light
        collectRenderCall(scene, lights[i]->camera, & lights[i]->rc);
        // sorting by alpha
        std::sort(lights[i]->rc.begin(), lights[i]->rc.end(), RenderCall::sorting_renderCalls);
        
        // Rendering the depth buffer to texture
        renderLightDepthBuffer(lights[i], lights[i]->rc);
    }
    
    // Rendering to framebuffer
    //set the clear color (the background color)
    glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

    // Clear the color and the depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    checkGLErrors();

    for (int i = 0; i < render_call_vector.size(); i++){
        renderMeshWithMaterial(render_call_vector[i]->model, render_call_vector[i]->mesh, render_call_vector[i]->material, camera);
    }
    
    // View the depth buffer of a light
    viewDepthBuffer(lights[this->selected_light]);
    
    //Clear rendercall for framebuffer
    clearRenderCall(& this->render_call_vector);
    // Clear rendercall for every light depth texture
    for (int i = 0; i < lights.size(); i++){
        clearRenderCall(& lights[i]->rc);
    }

}


void Renderer::collectRenderCall(GTR::Scene* scene, Camera* camera, std::vector<RenderCall*>* rc_vector){
	//render entities
	for (int i = 0; i < scene->entities.size(); ++i)
	{
		BaseEntity* ent = scene->entities[i];
		if (!ent->visible)
			continue;

		//is a prefab!
		if (ent->entity_type == PREFAB)
		{
			PrefabEntity* pent = (GTR::PrefabEntity*)ent;
			if(pent->prefab)
				collectPrefabInRenderCall(ent->model, pent->prefab, camera, rc_vector);
		}
	}
}

void Renderer::clearRenderCall(std::vector<RenderCall*>* rc_vector){
	rc_vector->clear();
}



//renders all the prefab
void Renderer::collectPrefabInRenderCall(const Matrix44& model, GTR::Prefab* prefab, Camera* camera, std::vector<RenderCall*>* rc_vector)
{
	assert(prefab && "PREFAB IS NULL");
	//assign the model to the root node
	collectNodesInRenderCall(model, &prefab->root, camera, rc_vector);
}

//renders a node of the prefab and its children
void Renderer::collectNodesInRenderCall(const Matrix44& prefab_model, GTR::Node* node, Camera* camera, std::vector<RenderCall*>* rc_vector)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model,node->mesh->box);
		
		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) )
		{
			//render node mesh
			//renderMeshWithMaterial( node_model, node->mesh, node->material, camera );
			RenderCall* rc = new RenderCall(&node_model, node->mesh, node->material, 10.0f); // De momento forzamos un mismo número de distance to camera
			rc_vector->push_back(rc);
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		collectNodesInRenderCall(prefab_model, node->children[i], camera, rc_vector);
}

void Renderer::renderToTexture(Camera* camera, FBO* fbo, std::vector<RenderCall*> rc_vector){
    fbo->bind();
   
    //you can disable writing to the color buffer to speed up the rendering as we do not need it
    glColorMask(false,false,false,false);

    //clear the depth buffer only (don't care of color)
    glClear( GL_DEPTH_BUFFER_BIT );
    checkGLErrors();

    
    for (int i = 0; i<rc_vector.size(); i++){
        renderMesh(rc_vector[i]->model, rc_vector[i]->mesh, camera, rc_vector[i]->material->alpha_mode);
    }
    fbo->unbind();
    
    //allow to render back to the color buffer
    glColorMask(true,true,true,true);

}

void Renderer::renderLightDepthBuffer(LightEntity* light, std::vector<RenderCall*> rc_vector){
    renderToTexture(light->camera, light->fbo, rc_vector);
}

void Renderer::viewDepthBuffer(LightEntity* light){
    //remember to disable ztest if rendering quads!
    glDisable(GL_DEPTH_TEST);
    FBO* fbo = light->fbo;
    //to use a special shader
    Shader* zshader = Shader::Get("depth");
    zshader->enable();
    zshader->setUniform("u_camera_nearfar", Vector2(light->camera->near_plane, light->camera->far_plane));
    
    // To render in a portion of the screen
    int w = Application::instance->window_width;
    int h = Application::instance->window_height;
    glViewport(0, 0, w*0.5, h*0.5);
    fbo->depth_texture->toViewport(zshader);
    
    zshader->disable();
    
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderMesh(const Matrix44 model, Mesh* mesh, Camera* camera, eAlphaMode material_alpha_mode){
    
    glDisable(GL_BLEND);
    //in case there is nothing to do
    if (!mesh || !mesh->getNumVertices())
        return;
    assert(glGetError() == GL_NO_ERROR);
    
    Shader* shader = NULL;
    
    // If blending, then we won't draw anything
    if (material_alpha_mode == GTR::eAlphaMode::BLEND)
    {
        return;
    }

    //chose a shader
    shader = Shader::Get("mesh");

    assert(glGetError() == GL_NO_ERROR);

    //no shader? then nothing to render
    if (!shader)
        return;
    shader->enable();
    
    //upload uniforms
    shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
    shader->setUniform("u_camera_position", camera->eye);
    shader->setUniform("u_model", model );
    
    mesh->render(GL_TRIANGLES);
    
    //disable shader
    shader->disable();
    
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	GTR::Scene* scene = GTR::Scene::instance;
    std::vector<Texture*> texture = std::vector<Texture*>(5);
    std::vector<GTR::LightEntity*> light_entities = scene->light_entities;
    bool has_emissive_light = true;    

    // Define Textures
	texture[0] = material->color_texture.texture;
	texture[1] = material->emissive_texture.texture;
    if (texture[1] == NULL)
        has_emissive_light = false;
	texture[2] = material->metallic_roughness_texture.texture;
	texture[3] = material->normal_texture.texture;
	texture[4] = material->occlusion_texture.texture;
	for (int t = 0; t < texture.size(); t++){
		if (texture[t] == NULL)
            texture[t] = Texture::getWhiteTexture(); //a 1x1 white texture
	}

	//select the blending
	if (material->alpha_mode == GTR::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if(material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
    assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get(this->shader_name.c_str());

    assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model );

	shader->setUniform("u_color", material->color);
    shader->setUniform("u_has_emissive_light", has_emissive_light);
    shader->setUniform("u_color_texture", texture[0], 0);
    shader->setUniform("u_emissive_texture", texture[1], 1);
    shader->setUniform("u_emissive_factor", material->emissive_factor);
    shader->setUniform("u_metallic_roughness_texture", texture[2], 2);
    shader->setUniform("u_normal_texture", texture[3], 3);
    shader->setUniform("u_occlusion_texture", texture[4], 4);
     

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::eAlphaMode::MASK ? material->alpha_cutoff : 0);
    
    shader->setUniform("u_ambient_light", scene->ambient_light);
    
	// Single pass
	if(multiple_light_rendering == SINGLEPASS) {
        singlepassRendering(light_entities, shader, mesh);
	}
    else if (multiple_light_rendering == MULTIPASS){
        multipassRendering(light_entities, shader, mesh, material);
    }
    else {
        // Use only the first light
        light_entities[0]->setUniforms(shader);
		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);
    }

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}


Texture* GTR::CubemapFromHDRE(const char* filename)
{
	HDRE* hdre = new HDRE();
	if (!hdre->load(filename))
	{
		delete hdre;
		return NULL;
	}

	/*
	Texture* texture = new Texture();
	texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFaces(0), hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_FLOAT );
	for(int i = 1; i < 6; ++i)
		texture->uploadCubemap(texture->format, texture->type, false, (Uint8**)hdre->getFaces(i), GL_RGBA32F, i);
	return texture;
	*/
	return NULL;
}

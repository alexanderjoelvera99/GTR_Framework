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
}

void Renderer::renderToTexture(Scene* scene, Camera* camera, FBO* fbo){
	fbo->bind();
	renderScene(scene, camera);
	fbo->unbind();
}

void Renderer::renderLightDepthBuffer(Scene* scene, LightEntity* light){
	FBO* fbo = light->fbo;
	renderToTexture(scene, light->camera, fbo);
    //remember to disable ztest if rendering quads!
    glDisable(GL_DEPTH_TEST);

	viewDepthBuffer(light);
}

void Renderer::viewDepthBuffer(LightEntity* light){
	FBO* fbo = light->fbo;
	//to use a special shader
    Shader* zshader = Shader::Get("depth");
    zshader->enable();
    zshader->setUniform("u_camera_nearfar", Vector2(light->camera->near_plane, light->camera->far_plane));
    fbo->depth_texture->toViewport(zshader);
    zshader->disable();
}

void Renderer::multipassRendering(std::vector<LightEntity*> lights, Shader* shader, Mesh* mesh){
	int num_lights = (int)lights.size();
	
	//allow to render pixels that have the same depth as the one in the depth buffer
	glDepthFunc( GL_LEQUAL );

	//set blending mode to additive
	//this will collide with materials with blend...
	glBlendFunc( GL_SRC_ALPHA,GL_ONE );

	for(int i = 0; i < num_lights; ++i)
	{
		//first pass doesn't use blending
        if(i == 0)
            glDisable( GL_BLEND );
        
        else{
            glEnable( GL_BLEND );
            shader->setUniform("u_emissive_factor", Vector3(0,0,0));
            shader->setUniform("u_ambient_light", Vector3(0,0,0));
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
	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGLErrors();

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
				renderPrefab(ent->model, pent->prefab, camera);
		}
	}
}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera)
{
	assert(prefab && "PREFAB IS NULL");
	//assign the model to the root node
	renderNode(model, &prefab->root, camera);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera)
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
			renderMeshWithMaterial( node_model, node->mesh, node->material, camera );
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera);
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
	std::vector<Texture*> texture = std::vector<Texture*>(5);
	GTR::Scene* scene = GTR::Scene::instance;
    std::vector<GTR::LightEntity*> light_entities = scene->light_entities;
    bool has_emissive_light = true;
    bool has_normal_texture = true;
    int number_of_lights = (int)light_entities.size();

    // Define Textures
	texture[0] = material->color_texture.texture;
	texture[1] = material->emissive_texture.texture;
    if (texture[1] == NULL)
        has_emissive_light = false;
	texture[2] = material->metallic_roughness_texture.texture;
	texture[3] = material->normal_texture.texture;
    if(texture[3] == NULL)
        has_normal_texture = false;
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
    shader->setUniform("u_has_normal_texture", has_normal_texture);
    shader->setUniform("u_occlusion_texture", texture[4], 4);
     

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::eAlphaMode::MASK ? material->alpha_cutoff : 0);
    
    shader->setUniform("u_ambient_light", scene->ambient_light);
    
	// Single pass
	if(multiple_light_rendering == SINGLEPASS) {
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
    else if (multiple_light_rendering == MULTIPASS){
        multipassRendering(light_entities, shader, mesh);
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

#include "scene.h"
#include "utils.h"

#include "prefab.h"
#include "extra/cJSON.h"
#include "application.h"

GTR::Scene* GTR::Scene::instance = NULL;

GTR::Scene::Scene()
{
	instance = this;
	
}

void GTR::Scene::clear()
{
	for (int i = 0; i < entities.size(); ++i)
	{
		BaseEntity* ent = entities[i];
		delete ent;
	}
    for (int i = 0; i < light_entities.size(); i++){
        BaseEntity* ent = light_entities[i];
        delete ent;
    }
	entities.resize(0);
    light_entities.resize(0);
}


void GTR::Scene::addEntity(BaseEntity* entity)
{
    if(entity->entity_type == LIGHT)
        light_entities.push_back((LightEntity*)entity);
    else
        entities.push_back(entity);
    entity->scene = this;
}

bool GTR::Scene::load(const char* filename)
{
	std::string content;

	this->filename = filename;
	std::cout << " + Reading scene JSON: " << filename << "..." << std::endl;

	if (!readFile(filename, content))
	{
		std::cout << "- ERROR: Scene file not found: " << filename << std::endl;
		return false;
	}

	//parse json string 
	cJSON* json = cJSON_Parse(content.c_str());
	if (!json)
	{
		std::cout << "ERROR: Scene JSON has errors: " << filename << std::endl;
		return false;
	}

	//read global properties
	background_color = readJSONVector3(json, "background_color", background_color);
	ambient_light = readJSONVector3(json, "ambient_light", ambient_light );
	main_camera.eye = readJSONVector3(json, "camera_position", main_camera.eye);
	main_camera.center = readJSONVector3(json, "camera_target", main_camera.center);
	main_camera.fov = readJSONNumber(json, "camera_fov", main_camera.fov);

	//entities
	cJSON* entities_json = cJSON_GetObjectItemCaseSensitive(json, "entities");
	cJSON* entity_json;
	cJSON_ArrayForEach(entity_json, entities_json)
	{
		std::string type_str = cJSON_GetObjectItem(entity_json, "type")->valuestring;
		BaseEntity* ent = createEntity(type_str);
		if (!ent)
		{
			std::cout << " - ENTITY TYPE UNKNOWN: " << type_str << std::endl;
			//continue;
			ent = new BaseEntity();
		}

		addEntity(ent);

		if (cJSON_GetObjectItem(entity_json, "name"))
		{
			ent->name = cJSON_GetObjectItem(entity_json, "name")->valuestring;
			stdlog(std::string(" + entity: ") + ent->name);
		}

		//read transform
		if (cJSON_GetObjectItem(entity_json, "position"))
		{
			ent->model.setIdentity();
			Vector3 position = readJSONVector3(entity_json, "position", Vector3());
			ent->model.translate(position.x, position.y, position.z);
		}

		if (cJSON_GetObjectItem(entity_json, "angle"))
		{
			float angle = cJSON_GetObjectItem(entity_json, "angle")->valuedouble;
			ent->model.rotate(angle * DEG2RAD, Vector3(0, 1, 0));
		}

		if (cJSON_GetObjectItem(entity_json, "rotation"))
		{
			Vector4 rotation = readJSONVector4(entity_json, "rotation");
			Quaternion q(rotation.x, rotation.y, rotation.z, rotation.w);
			Matrix44 R;
			q.toMatrix(R);
			ent->model = R * ent->model;
		}

		if (cJSON_GetObjectItem(entity_json, "target"))
		{
			Vector3 target = readJSONVector3(entity_json, "target", Vector3());
			Vector3 front = target - ent->model.getTranslation();
			ent->model.setFrontAndOrthonormalize(front);
		}

		if (cJSON_GetObjectItem(entity_json, "scale"))
		{
			Vector3 scale = readJSONVector3(entity_json, "scale", Vector3(1, 1, 1));
			ent->model.scale(scale.x, scale.y, scale.z);
		}

		ent->configure(entity_json);
	}

	//free memory
	cJSON_Delete(json);

	return true;
}

GTR::BaseEntity* GTR::Scene::createEntity(std::string type)
{
	if (type == "PREFAB")
		return new GTR::PrefabEntity();
    else if(type == "LIGHT"){
        return new GTR::LightEntity();
    }
    return NULL;
}

void GTR::BaseEntity::renderInMenu()
{
#ifndef SKIP_IMGUI
	ImGui::Text("Name: %s", name.c_str()); // Edit 3 floats representing a color
	ImGui::Checkbox("Visible", &visible); // Edit 3 floats representing a color
	//Model edit
	ImGuiMatrix44(model, "Model");
#endif
}




GTR::PrefabEntity::PrefabEntity()
{
	entity_type = PREFAB;
	prefab = NULL;
}

void GTR::PrefabEntity::configure(cJSON* json)
{
	if (cJSON_GetObjectItem(json, "filename"))
	{
		filename = cJSON_GetObjectItem(json, "filename")->valuestring;
		prefab = GTR::Prefab::Get( (std::string("data/") + filename).c_str());
	}
}

void GTR::PrefabEntity::renderInMenu()
{
	BaseEntity::renderInMenu();

#ifndef SKIP_IMGUI
	ImGui::Text("filename: %s", filename.c_str()); // Edit 3 floats representing a color
	if (prefab && ImGui::TreeNode(prefab, "Prefab Info"))
	{
		prefab->root.renderInMenu();
		ImGui::TreePop();
	}
#endif
}

GTR::LightEntity::LightEntity() : GTR::BaseEntity(){
	entity_type = LIGHT;
	// default values
	this->color = Vector3(0.0f, 0.0f, 0.0f);
	this->light_type = DIRECTIONAL;
	this->max_distance = 0;
	this->cone_angle = 0;
	this->camera_light = new Camera();
	this->fbo = new FBO();
	this->fbo->create(Application::instance->window_width, Application::instance->window_height);
    this->shadow_bias = 0.0001;
}

GTR::LightEntity::~LightEntity(){}

// To change the light of the color
void GTR::LightEntity::changeLightColor(Vector3 delta){
	color += delta;
	color.setMax(Vector3(0.0f, 0.0f, 0.0f));
	color.setMin(Vector3(1.0f, 1.0f, 1.0f));
}

void GTR::LightEntity::changeLightPosition(Vector3 delta){
    this->model.translate(delta[0], -delta[1], delta[2]);
    this->camera_light->move(delta);
}

void GTR::LightEntity::setUniforms(Shader* shader){
	// Light properties uniforms
    shader->setUniform("u_light_color", this->color);
	shader->setUniform("u_light_position",this->model.getTranslation());
	shader->setUniform("u_light_type",this->light_type);
	shader->setUniform("u_light_direction",this->model.frontVector());
	shader->setUniform("u_max_distance",this->max_distance);
	shader->setUniform("u_cone_angle",this->cone_angle);
    shader->setUniform("u_intensity", this->intensity);
    
    //Shadow map uniforms
    shader->setUniform("u_shadow_viewproj", this->camera_light->viewprojection_matrix);
    //shader->setUniform("u_shadow_camera_position", this->camera->eye);
    shader->setTexture("u_shadowmap", this->fbo->depth_texture, 8);
    shader->setUniform("u_shadow_bias", this->shadow_bias );
    shader->setUniform("u_cone_exp", this->cone_exp);
}

// Configuring special json fields for Light entity
void GTR::LightEntity::configure(cJSON* json)
{
	if (cJSON_GetObjectItem(json, "color"))
	{
		Vector3 light_color = readJSONVector3(json, "color", Vector3(0, 0, 0));
		this->color = light_color;
	}
	if (cJSON_GetObjectItem(json, "intensity"))
	{
		float intensity = readJSONNumber(json, "intensity", 0.0f);
		this->intensity = intensity;
	}
	if (cJSON_GetObjectItem(json, "light_type"))
	{
		std::string light_type_str = cJSON_GetObjectItem(json, "light_type")->valuestring;
		
		if(light_type_str.compare("POINT") == 0){
			this->light_type = POINT;
		}
		else if (light_type_str.compare("SPOT") == 0){
			this->light_type = SPOT;
		}
        else if (light_type_str.compare("DIRECTIONAL") == 0){
            this->light_type = DIRECTIONAL;
        }
        else {
            std::cout << "ERROR: Light type " << light_type_str << " not supported " << std::endl;
        }
	}

	if (cJSON_GetObjectItem(json, "target"))
		{
			Vector3 target = readJSONVector3(json, "target", Vector3(0,0,0));
            Vector3 light_position = this->model.getTranslation();
            Vector3 front_vector = target - light_position;
			this->model.setFrontAndOrthonormalize(front_vector);
		}

	if (cJSON_GetObjectItem(json, "max_dist"))
	{
		float max_distance = readJSONNumber(json, "max_dist", 0.0f);
		this->max_distance = max_distance;
	}
	if (cJSON_GetObjectItem(json, "cone_angle"))
	{
		float cone_angle = readJSONNumber(json, "cone_angle", 0.0f);
        this->cone_angle = (cone_angle*PI)/180; 
	}
    if (cJSON_GetObjectItem(json, "cone_exp"))
    {
        this->cone_exp = readJSONNumber(json, "cone_exp", 0.0f);
    }
    if (cJSON_GetObjectItem(json, "shadow_bias"))
    {
        this->shadow_bias = readJSONNumber(json, "shadow_bias", 0.0001f);
    }
    if (cJSON_GetObjectItem(json, "area_size"))
    {
        this->area_size = readJSONNumber(json, "area_size", 0);
    }
    
    setCameraLight();
    
}

void GTR::LightEntity::setCameraLight(){
    int w = Application::instance->window_height;
    int h = Application::instance->window_height;
    // Setting perspective for the camera to use in Shadow maps
    if(this->light_type == SPOT){
        float cone_angle_degrees = (this->cone_angle*180)/PI;
        float aspect = w/(float)h;
        camera_light->setPerspective( cone_angle_degrees, aspect, 1.0f, this->max_distance);
    }
    else if(this->light_type == DIRECTIONAL){
        camera_light->setOrthographic(-area_size/2, area_size/2, -area_size/2, area_size/2, -this->max_distance, this->max_distance);
    }
    else if(this->light_type == POINT){
        // De momento nada... Pero se puede applicar un cube map
    }
    
    // Set the camera look at
    setCameraAsLight();
}

void GTR::LightEntity::setCameraAsLight(){
	Vector3 light_position = this->model.getTranslation();
	Vector3 light_front = this->model.frontVector();
	camera_light->lookAt(light_position, light_position + light_front, Vector3(0.0f,1.0f,0.0f)); // We locate the camera as the light. Also, we apply the same front vector.
}

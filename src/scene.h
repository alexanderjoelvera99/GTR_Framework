#ifndef SCENE_H
#define SCENE_H

#include "framework.h"
#include "shader.h"
#include "camera.h"
#include <string>
#include "camera.h"

//forward declaration
class cJSON; 


//our namespace
namespace GTR {



	enum eEntityType {
		NONE = 0,
		PREFAB = 1,
		LIGHT = 2,
		CAMERA = 3,
		REFLECTION_PROBE = 4,
		DECALL = 5
	};

	enum eLightType{
			DIRECTIONAL = 0,
			POINT=1,
			SPOT=2,
			
		};

	class Scene;
	class Prefab;

	//represents one element of the scene (could be lights, prefabs, cameras, etc)
	class BaseEntity
	{
	public:
		Scene* scene;
		std::string name;
		eEntityType entity_type;
		Matrix44 model;
		bool visible;
		BaseEntity() { entity_type = NONE; visible = true; }
		virtual ~BaseEntity() {}
		virtual void renderInMenu();
		virtual void configure(cJSON* json) {}
	};

	//represents one prefab in the scene
	class PrefabEntity : public GTR::BaseEntity
	{
	public:
		std::string filename;
		Prefab* prefab;
		
		PrefabEntity();
		virtual void renderInMenu();
		virtual void configure(cJSON* json);
	};

	//represents one light in the scene
	class LightEntity : public GTR::BaseEntity
	{
	public:
		static LightEntity* instance;
		//Atributes
		//...to define ...
		Vector3 color;
		float intensity;
		eLightType light_type;
		Vector3 target;
		float max_distance;
		float cone_angle;
		float area_size;
		Camera* camera; // For shadow maps
		
		//Constructor
		LightEntity();
		//Destructor
		~LightEntity();

		//Methods
		void changeLightColor(Vector3 delta);
		void changeLightPosition(Vector3 delta);
		void configure(cJSON* json);
		void setUniforms(Shader* shader);
		void setCameraAsLight();
	};

	//contains all entities of the scene
	class Scene
	{
	public:
		static Scene* instance;

		Vector3 background_color;
		Vector3 ambient_light;
		Camera main_camera;

		Scene();

		std::string filename;
		std::vector<BaseEntity*> entities;
		std::vector<LightEntity*> light_entities;

		void clear();
		void addEntity(BaseEntity* entity);

		bool load(const char* filename);
		BaseEntity* createEntity(std::string type);
		void changeAmbientLightColor(Vector3 delta);
	};

};

#endif

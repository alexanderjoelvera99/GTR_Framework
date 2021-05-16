#include "renderCall.h"

// Constructor
GTR::RenderCall::RenderCall(Matrix44* model, Mesh* mesh, Material* material, float distance_to_camera)
{  
    this->model = *model;
    this->mesh = mesh;
    this->material = material;
    this->distance_to_camera = distance_to_camera;
}
    
// desrtuctor
//GTR::RenderCall::~RenderCall(){}


// Operators
bool GTR::RenderCall::operator == (RenderCall& rc_b){
    return this->material->alpha_mode == rc_b.material->alpha_mode;
}

bool GTR::RenderCall::operator > (RenderCall& rc_b){
    return this->material->alpha_mode > rc_b.material->alpha_mode;
}

bool GTR::RenderCall::operator < (RenderCall& rc_b){
    return this->material->alpha_mode > rc_b.material->alpha_mode;
}

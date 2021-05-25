#ifndef RENDERCALL_H
#define RENDERCALL_H
#include "framework.h"
#include "mesh.h"
#include "material.h"


namespace GTR{
    class RenderCall
        {
        public:
            Matrix44 model;
            Mesh* mesh;
            Material* material;
            float distance_to_camera;
    
            //constructor
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
}



#endif

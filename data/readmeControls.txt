I have been working on shader_atlas_osx.txt

light color controls:
* reduce red light component -> J key
* increase red light component -> U key
* reduce green light component -> K key
* increase green light component -> I key
* reduce blue light component -> L key
* increase blue light component -> O keyd

Light position controls:
* move to -x -> R
* move to +x -> F
* move to -y -> G
* move to +y -> T
* move to -z -> Y
* move to +z -> H

Change the light to control -> SPACE

Change multiple light rendering and shader -> 0
    Rendering options:
    * Multipass: Use "light" shader and MULTIPASS mode. You can initialize the renderer as follows
        renderer = new GTR::Renderer(GTR::MULTIPASS, "light");
    * Singlepass: Use "singlepass" shader and SINGLEPASS mode. You can initialize the renderer as follows
        renderer = new GTR::Renderer(GTR::SINGLEPASS, "singlepass");

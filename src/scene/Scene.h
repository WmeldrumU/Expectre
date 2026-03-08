#ifndef SCENE
#define SCENE

#include "scene/SceneObject.h"
#include "input/InputManager.h"
namespace Expectre
{
    class Scene
    {
    public:
        Scene();
        // Delete the copy constructor
        Scene(const Scene &other) = delete;
        // Delete the copy assignment operator as well for consistency
        Scene &operator=(const Scene &other) = delete;
        void Update(uint64_t delta_time, const InputManager& input_manager);
        SceneObject& GetRoot() { return *m_root; }
    private:

    void create_root();
        std::unique_ptr<SceneObject> m_root;
    };
}
#endif // SCENE
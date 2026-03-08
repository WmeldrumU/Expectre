#ifndef SCENE_OBJECT
#define SCENE_OBJECT
#include <glm/mat3x4.hpp>
#include <vector>
#include <memory>
#include <string>

namespace Expectre
{
    class SceneObject
    {
    protected:
        SceneObject() = default; // to be called by root scene object only

    public:
        // Scene object must be created with a parent
        SceneObject(SceneObject* parent, std::string name);
        // Delete the copy constructor
        SceneObject(const SceneObject& other) = delete;
        // Delete the copy assignment operator as well for consistency
        SceneObject& operator=(const SceneObject& other) = delete;

        void create_child(std::string name);
        void set_transform(const glm::mat3x4& transform)
        {
            m_world_transform = transform;
        }

        virtual glm::mat3x4 get_relative_transform()
        {
            return m_world_transform;
        }

        virtual glm::mat3x4 get_world_transform() {
            return m_relative_transform;
        }

        std::string get_name() {
            return m_name;
        }

        const std::vector<std::unique_ptr<SceneObject>>& get_children() {

        }
    protected:
        SceneObject* m_parent;
        std::string m_name;
        glm::mat3x4 m_world_transform{};
        glm::mat3x4 m_relative_transform{};
        std::vector<std::unique_ptr<SceneObject>> m_children;
        bool m_renderable = true;
    };
}
#endif // SCENE_OBJECT
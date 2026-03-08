#ifndef SCENE_OBJECT
#define SCENE_OBJECT
#include <glm/mat4x3.hpp>
#include <vector>
#include <memory>
#include <string>
#include "Mesh.h"

namespace Expectre
{
    class SceneObject
    {
    protected:
        SceneObject() = default; // to be called by root scene object only

    public:
        // Scene object must be created with a parent
        SceneObject(SceneObject *parent, std::string name);
        // Delete the copy constructor
        SceneObject(const SceneObject &other) = delete;
        // Delete the copy assignment operator as well for consistency
        SceneObject &operator=(const SceneObject &other) = delete;

        void set_transform(const glm::mat4x3 &transform)
        {
            m_world_transform = transform;
        }

        virtual glm::mat4x3 get_relative_transform()
        {
            return m_world_transform;
        }

        virtual glm::mat4x3 get_world_transform()
        {
            return m_relative_transform;
        }

        std::string get_name()
        {
            return m_name;
        }

        const std::vector<std::shared_ptr<SceneObject>> &get_children()
        {
            return m_children;
        }

        void add_child(std::shared_ptr<SceneObject> child)
        {
            m_children.push_back(child);
        }

        void set_mesh(const Mesh&& mesh)
        {
            
            m_has_mesh = mesh.vertices.size() > 0;
            m_mesh = mesh;
        }

        bool has_mesh()
        {
            return m_has_mesh;
        }

    protected:
        SceneObject *m_parent;
        std::string m_name;
        glm::mat4x3 m_world_transform{};
        glm::mat4x3 m_relative_transform{};
        Mesh m_mesh{};

        std::vector<std::shared_ptr<SceneObject>> m_children;
        bool m_renderable = true;
        bool m_has_mesh = false;
    };
}
#endif // SCENE_OBJECT
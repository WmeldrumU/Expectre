
#include "scene/SceneObject.h"
#include <stdexcept>

namespace Expectre
{

        SceneObject::SceneObject(SceneObject *parent, std::string name = "") : m_parent{parent}
        {
                if (parent == nullptr)
                {
                        throw std::runtime_error("parent cannot be \
                                null unless the current object is scene root.");
                }
                m_world_transform = parent->get_world_transform();
                m_relative_transform = glm::mat3x4{};

                if (name == "")
                {
                        m_name = parent->get_name() + "/" + std::to_string(parent->get_children().size());
                }
                else
                {
                        m_name = parent->get_name() + "/" + name;
                }
        }

}

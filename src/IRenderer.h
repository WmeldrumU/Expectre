#ifndef IRENDERER_H
#define IRENDERER_H

#include <memory>
#include <SDL3/SDL.h>

#include "RenderableInfo.h"
namespace Expectre {

class Camera;

class IRenderer {
public:
	virtual ~IRenderer() = default;
	virtual void draw_frame(const Camera& camera, const std::vector<RenderableInfo> &renderables) = 0;
	// Add other common methods here

	// virtual void update(uint64_t delta_time) = 0;
};

} // namespace Expectre

#endif // IRENDERER_H
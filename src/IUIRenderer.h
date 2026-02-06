#ifndef IUIRENDERER_H
#define IUIRENDERER_H

#include "IRendererInfo.h"

namespace Expectre {
	class IUIRenderer {
	public:
		IUIRenderer() = default;
		IUIRenderer(const IRendererInfo& renderer_info);
		IUIRenderer(const IUIRenderer&) = delete;
		IUIRenderer& operator=(const IUIRenderer&) = delete;

		// virtual void Draw(uint32_t current_frame) = 0;
		//virtual void Update(uint64_t total_time);
		virtual ~IUIRenderer() = default;

	private:

	};
}

#endif
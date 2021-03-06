// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace SceneEngine
{
    class LightingParserContext;

    void SunFlare_Execute(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext,
        RenderCore::Metal::ShaderResourceView& depthsSRV);
}


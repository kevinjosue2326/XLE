// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CommonResources.h"
#include "ResourceBox.h"
#include "TechniqueUtils.h" // just for sizeof(LocalTransformConstants)

namespace RenderCore { namespace Techniques
{
    namespace Internal
    {
        std::vector<std::unique_ptr<IBoxTable>> BoxTables;
        IBoxTable::~IBoxTable() {}
    }

    CommonResourceBox::CommonResourceBox(const Desc&)
    {
        using namespace RenderCore::Metal;
        _dssReadWrite = DepthStencilState();
        _dssReadOnly = DepthStencilState(true, false);
        _dssDisable = DepthStencilState(false, false);
        _dssReadWriteWriteStencil = DepthStencilState(true, true, 0xff, 0xff, StencilMode::AlwaysWrite, StencilMode::AlwaysWrite);
        _dssWriteOnly = DepthStencilState(true, true, Comparison::Always);

        _blendStraightAlpha = BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha);
        _blendAlphaPremultiplied = BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha);
        _blendOneSrcAlpha = BlendState(BlendOp::Add, Blend::One, Blend::SrcAlpha);
        _blendAdditive = BlendState(BlendOp::Add, Blend::One, Blend::One);
        _blendOpaque = BlendOp::NoBlending;

        _defaultRasterizer = CullMode::Back;
        _cullDisable = CullMode::None;
        _cullReverse = RasterizerState(CullMode::Back, false);

        _linearClampSampler = SamplerState(FilterMode::Trilinear, AddressMode::Wrap, AddressMode::Wrap);
        _linearClampSampler = SamplerState(FilterMode::Trilinear, AddressMode::Clamp, AddressMode::Clamp);
        _pointClampSampler = SamplerState(FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp);

        _localTransformBuffer = ConstantBuffer(nullptr, sizeof(LocalTransformConstants));
    }

    CommonResourceBox& CommonResources()
    {
        return FindCachedBox<CommonResourceBox>(CommonResourceBox::Desc());
    }


    void ResourceBoxes_Shutdown()
    {
        Internal::BoxTables = std::vector<std::unique_ptr<Internal::IBoxTable>>();
    }

}}

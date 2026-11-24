# Layout
add_library(Atom_Layout STATIC
    Engine/Presentation/Layout/LayoutConfig.cpp
    Engine/Presentation/Layout/LayoutConfig.hpp
    Engine/Presentation/Layout/LayoutNode.cpp
    Engine/Presentation/Layout/LayoutNode.hpp
    Engine/Presentation/Layout/LayoutTree.cpp
    Engine/Presentation/Layout/LayoutTree.hpp
    Engine/Presentation/Layout/LayoutTypes.hpp
)
target_include_directories(Atom_Layout PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")
target_link_libraries(Atom_Layout PRIVATE Atom_3rd_Yoga Atom_Log)

# Text — backend-neutral font provider and glyph rasterization service.
add_library(Atom_Text STATIC
    Engine/Presentation/Render/Text/Font.cpp
    Engine/Presentation/Render/Text/Font.hpp
    Engine/Presentation/Render/Text/FontFileLoader.cpp
    Engine/Presentation/Render/Text/FontFileLoader.hpp
    Engine/Presentation/Render/Text/FreeTypeFontProvider.cpp
    Engine/Presentation/Render/Text/FreeTypeFontProvider.hpp
    Engine/Presentation/Render/Text/HarfBuzzTextShaper.cpp
    Engine/Presentation/Render/Text/HarfBuzzTextShaper.hpp
    Engine/Presentation/Render/Text/TextLayoutBuilder.cpp
    Engine/Presentation/Render/Text/TextLayoutBuilder.hpp
)
target_include_directories(Atom_Text PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")
target_link_libraries(Atom_Text PRIVATE Atom_3rd_FreeType Atom_3rd_HarfBuzz Atom_Concurrency)
target_link_libraries(Atom_Text PUBLIC Atom_Assets)

# Pipeline — backend-neutral pipeline descriptors shared by 2D and 3D.
add_library(Atom_Render_Pipeline INTERFACE)
target_sources(Atom_Render_Pipeline INTERFACE
    Engine/Presentation/Render/Pipeline/PipelineTypes.hpp
)
target_include_directories(Atom_Render_Pipeline INTERFACE "${PROJECT_SOURCE_DIR}/Engine/Presentation")

# Render Core — backend-neutral pass scheduling and render orchestration.
add_library(Atom_Render_Core STATIC
    Engine/Presentation/Render/Core/RenderGraph.cpp
    Engine/Presentation/Render/Core/RenderGraph.hpp
)
target_include_directories(Atom_Render_Core PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")

# Renderer2D — high-level batched 2D renderer on the IRender2DContext slice.
add_library(Atom_Renderer2D STATIC
    Engine/Presentation/Render/Renderer2D/Renderer2D.cpp
    Engine/Presentation/Render/Renderer2D/Renderer2D.hpp
)
target_include_directories(Atom_Renderer2D PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")
target_link_libraries(Atom_Renderer2D PUBLIC Atom_Backend_Contracts Atom_Render_Pipeline Atom_Render_Core Atom_Text)
target_link_libraries(Atom_Renderer2D PRIVATE Atom_Log)

# Game UI — retained widgets over Layout, normalized input, and a
# screen-space Renderer2D canvas. Runtime-owned pages may consume this module;
# UI itself has no dependency on Runtime, Window implementations, ECS, or ImGui.
add_library(Atom_UI STATIC
    Engine/Presentation/UI/UiTree.hpp
    Engine/Presentation/UI/UiTree.cpp
    Engine/Presentation/UI/UiCanvas.hpp
    Engine/Presentation/UI/UiCanvas.cpp
)
target_include_directories(Atom_UI PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")
target_link_libraries(Atom_UI PUBLIC Atom_Layout Atom_InputSystem Atom_Renderer2D)

# Render resources — bridges decoded media assets into renderer-owned GPU resources.
add_library(Atom_Render_Resources STATIC
    Engine/Presentation/Render/Resources/ImageTexture.hpp
    Engine/Presentation/Render/Resources/ImageTexture.cpp
    Engine/Presentation/Render/Resources/TextureCache.hpp
    Engine/Presentation/Render/Resources/TextureCache.cpp
)
target_include_directories(Atom_Render_Resources PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")
target_link_libraries(Atom_Render_Resources PUBLIC Atom_Renderer2D Atom_Image)
target_link_libraries(Atom_Render_Resources PUBLIC Atom_FS)

# Async single-image loads (TextureCache::LoadAsync) decode on the job
# system. JobSystem is an implementation detail of TextureCache.cpp, so the
# link stays PRIVATE; consumers only see JobHandle by value in the header.
target_link_libraries(Atom_Render_Resources PRIVATE Atom_Concurrency)

# Scene2D — ECS sprites and a 2D camera turned into one Renderer2D pass.
#     Depends on ECS + Renderer2D + render resources, and never on Runtime: a
#     scene renderer must be usable by a tool or a test with no frame loop.
add_library(Atom_Scene2D STATIC
    Engine/Presentation/Render/Scene2D/Scene2DTypes.hpp
    Engine/Presentation/Render/Scene2D/Scene2DRenderer.hpp
    Engine/Presentation/Render/Scene2D/Scene2DRenderer.cpp
    Engine/Presentation/Render/Scene2D/TextureCacheResolver.hpp
    Engine/Presentation/Render/Scene2D/TextureCacheResolver.cpp
)
target_include_directories(Atom_Scene2D PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")
target_link_libraries(Atom_Scene2D PUBLIC Atom_ECS Atom_Renderer2D Atom_Render_Resources)

# Sprite — the art pipeline's metadata layer: one texture, named frames,
#     one filter. Depends on Json + FS + Math/Color, NOT on ECS: an atlas is asset
#     data, and a component must not need to know what an atlas is.
add_library(Atom_Sprite STATIC
    Engine/Presentation/Sprite/SpriteAtlas.hpp
    Engine/Presentation/Sprite/SpriteAtlas.cpp
    Engine/Presentation/Sprite/SpriteAtlasLoader.hpp
    Engine/Presentation/Sprite/SpriteAtlasLoader.cpp
)
target_include_directories(Atom_Sprite PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Presentation")
target_link_libraries(Atom_Sprite PUBLIC Atom_Json Atom_FS Atom_Log Atom_Math Atom_Color)

/**
 * @file           : ScenePrefabTest.cpp
 * @brief          : T2.3 - flat prefabs and the override whitelist.
 *
 * A prefab is a reusable entity *body*; identity and per-instance variation live in
 * the scene. The rules this test locks down, and why each one matters:
 *
 *   - two instances of one prefab must not pollute each other (the classic bug: an
 *     override written back into the shared base);
 *   - an override may move/tint/layer/re-time an entity but must not change what it
 *     *is* - overriding `sprite.texture` is refused rather than silently applied;
 *   - a prefab that fails to load fails the whole scene load, so a typo cannot leave
 *     a hole in the level;
 *   - nesting is not supported in v1, which is also why cycles cannot exist.
 *
 * Prefabs are supplied through the provider callback rather than the VFS, so the
 * merge logic is tested without a filesystem; the shipped prefab file is covered
 * separately by Atom_Scene.SceneLoader.
 */

#include <Scene/SceneSchema.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace atom::scene;

// A provider backed by an in-memory map, mimicking what the loader does with the VFS.
class MapProvider final {
    public:
        auto Add(std::string uri, std::string document) -> void {
            documents_.emplace(std::move(uri), std::move(document));
        }

        [[nodiscard]] auto Provider() const -> PrefabProvider {
            return [this](const std::string_view uri) -> atom::Result<EntityDesc> {
                const auto found = documents_.find(std::string{uri});
                if (found == documents_.end()) {
                    return std::unexpected<atom::Error>{
                        atom::MakeError(atom::ErrorCode::SceneLoadFailed, atom::Module::Scene, "test provider")
                            .WithDetail("no prefab at " + std::string{uri})};
                }
                auto outcome = ParsePrefab(found->second, uri);
                if (!outcome.Ok()) {
                    return std::unexpected<atom::Error>{
                        atom::MakeError(atom::ErrorCode::SceneInvalidField, atom::Module::Scene, "test provider")
                            .WithDetail(outcome.diagnostics.front().Format())};
                }
                return *outcome.entity;
            };
        }

    private:
        std::map<std::string, std::string> documents_{};
};

// A minimal crate prefab: static, one 48x48 sprite, no camera, no lifetime.
constexpr std::string_view kCratePrefab = R"({
  "schema_version": 1,
  "kind": "prefab",
  "id": "crate",
  "entity": {
    "name": "crate",
    "tags": ["static"],
    "transform": { "position": [0.0, 0.0] },
    "sprite": {
      "texture": "res://textures/crate.png",
      "size": [48.0, 48.0],
      "pivot": [0.5, 0.5],
      "tint": [255, 255, 255, 255],
      "layer": 0,
      "sort_key": 0
    }
  }
})";

auto Scene(std::string_view body) -> std::string {
    return "{\n  \"schema_version\": 1,\n  \"id\": \"test\",\n  \"entities\": [" + std::string{body} + "]\n}";
}

auto HasDiagnostic(const ParseOutcome& outcome, const std::string_view needle) -> bool {
    for (const auto& diagnostic : outcome.diagnostics) {
        if (diagnostic.Format().find(needle) != std::string::npos)
            return true;
    }
    return false;
}

auto Fails(const std::string_view body, const PrefabProvider& provider, const std::string_view expected) -> bool {
    const auto outcome = ParseScene(Scene(body), "res://scenes/test.scene.json", provider);
    if (outcome.Ok())
        return false;
    return HasDiagnostic(outcome, expected);
}

} // namespace

auto main() -> int {
    MapProvider prefabs{};
    prefabs.Add("res://prefabs/crate.prefab.json", std::string{kCratePrefab});
    const auto provider = prefabs.Provider();

    // --- 1. A prefab document parses into a body without identity ---------
    {
        const auto outcome = ParsePrefab(kCratePrefab, "res://prefabs/crate.prefab.json");
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        const auto& body = *outcome.entity;
        // No identity: the instance supplies the id.
        ATOM_CHECK(body.id == 0);
        ATOM_CHECK(body.has_name && body.name == "crate");
        ATOM_CHECK(body.is_static);
        ATOM_CHECK(body.sprite.has_value());
        ATOM_CHECK(body.sprite->texture_uri == "res://textures/crate.png");
        ATOM_CHECK(body.sprite->has_size);
        ATOM_CHECK(!body.camera.has_value());
        ATOM_CHECK(!body.lifetime.has_value());
    }

    // --- 2. Instances inherit the body and apply only what they override ---
    {
        const auto outcome = ParseScene(Scene(R"({
            "id": 1,
            "prefab": "res://prefabs/crate.prefab.json",
            "overrides": {
                "name": "crate_left",
                "transform": { "position": [100.0, 200.0] },
                "sprite": { "sort_key": 10 }
            }
        })"),
                                        "res://scenes/test.scene.json", provider);
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        ATOM_CHECK(outcome.scene->entities.size() == 1);
        const auto& entity = outcome.scene->entities.front();
        ATOM_CHECK(entity.id == 1);
        // Overridden fields...
        ATOM_CHECK(entity.name == "crate_left");
        ATOM_CHECK_NEAR(entity.transform.position.GetX(), 100.0F);
        ATOM_CHECK_NEAR(entity.transform.position.GetY(), 200.0F);
        ATOM_CHECK(entity.sprite->sort_key == 10);
        // ...and inherited ones, still the prefab's.
        ATOM_CHECK(entity.is_static);
        ATOM_CHECK(entity.sprite->texture_uri == "res://textures/crate.png");
        ATOM_CHECK(entity.sprite->pivot.GetX() == 0.5F);
        ATOM_CHECK(entity.sprite->layer == 0);
    }

    // --- 3. Two instances do not pollute each other -----------------------
    //
    // This is the bug the design has to prevent: if the merge wrote back into a shared
    // base, instance B would inherit instance A's position.
    {
        const auto outcome = ParseScene(Scene(R"({
            "id": 1, "prefab": "res://prefabs/crate.prefab.json",
            "overrides": { "transform": { "position": [10.0, 20.0] }, "sprite": { "sort_key": 1 } }
        }, {
            "id": 2, "prefab": "res://prefabs/crate.prefab.json",
            "overrides": { "transform": { "position": [30.0, 40.0] }, "sprite": { "sort_key": 2 } }
        })"),
                                        "res://scenes/test.scene.json", provider);
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        ATOM_CHECK(outcome.scene->entities.size() == 2);
        const auto& first = outcome.scene->entities[0];
        const auto& second = outcome.scene->entities[1];
        ATOM_CHECK_NEAR(first.transform.position.GetX(), 10.0F);
        ATOM_CHECK_NEAR(second.transform.position.GetX(), 30.0F);
        ATOM_CHECK(first.sprite->sort_key == 1);
        ATOM_CHECK(second.sprite->sort_key == 2);

        // A third instance with *no* overrides must see the prefab's own defaults, not
        // the previous instance's values.
        const auto plain = ParseScene(Scene(R"({ "id": 3, "prefab": "res://prefabs/crate.prefab.json" })"),
                                      "res://scenes/test.scene.json", provider);
        ATOM_CHECK_MSG(plain.Ok(), plain.Ok() ? "" : plain.diagnostics.front().Format().c_str());
        ATOM_CHECK_NEAR(plain.scene->entities.front().transform.position.GetX(), 0.0F);
        ATOM_CHECK(plain.scene->entities.front().sprite->sort_key == 0);
        ATOM_CHECK(plain.scene->entities.front().name == "crate");
    }

    // --- 4. Tags replace, they do not union -------------------------------
    {
        const auto outcome = ParseScene(Scene(R"({
            "id": 1, "prefab": "res://prefabs/crate.prefab.json",
            "overrides": { "tags": ["active"] }
        })"),
                                        "res://scenes/test.scene.json", provider);
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        const auto& entity = outcome.scene->entities.front();
        // The prefab said static; the instance said active. Replace means exactly one.
        ATOM_CHECK(entity.active);
        ATOM_CHECK(!entity.is_static);
    }

    // --- 5. Identity fields cannot be overridden --------------------------
    //
    // "What the entity is" is the prefab's business. Each of these is a whitelist
    // rejection naming the field, so the author learns the rule from the error.
    {
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "overrides": { "sprite": { "texture": "res://textures/other.png" } } })",
                         provider, "unknown field \"texture\""));
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "overrides": { "sprite": { "source": [0, 0, 8, 8] } } })",
                         provider, "unknown field \"source\""));
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "overrides": { "sprite": { "size": [10, 10] } } })",
                         provider, "unknown field \"size\""));
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "overrides": { "sprite": { "pivot": [0, 0] } } })",
                         provider, "unknown field \"pivot\""));
        // Overriding a block the prefab does not have is a different mistake and gets
        // its own message.
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "overrides": { "camera": { "zoom": 2 } } })",
                         provider, "no camera"));
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "overrides": { "lifetime": -1 } })",
                         provider, "must not be negative"));
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "overrides": { "nmae": "x" } })",
                         provider, "did you mean \"name\"?"));
    }

    // --- 6. An instance is { id, prefab, overrides } and nothing else -----
    {
        // A body field on an instance would be a second source for the same datum.
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json",
                               "sprite": { "texture": "res://textures/crate.png" } })",
                         provider, "unknown field \"sprite\""));
        // `overrides` without a prefab has nothing to override.
        ATOM_CHECK(Fails(R"({ "id": 1, "overrides": { "name": "x" } })", provider, "requires a \"prefab\""));
        // A prefab reference must be a res:// URI.
        ATOM_CHECK(Fails(R"({ "id": 1, "prefab": "prefabs/crate.prefab.json" })", provider, "res://"));
        // No provider means no way to resolve it: an error, not an empty entity.
        const auto no_provider = ParseScene(Scene(R"({ "id": 1, "prefab": "res://prefabs/crate.prefab.json" })"),
                                            "res://scenes/test.scene.json");
        ATOM_CHECK(!no_provider.Ok());
        ATOM_CHECK(HasDiagnostic(no_provider, "no prefab provider"));
    }

    // --- 7. A broken prefab fails the scene, and says which document ------
    {
        const auto missing = ParseScene(Scene(R"({ "id": 1, "prefab": "res://prefabs/nope.prefab.json" })"),
                                        "res://scenes/test.scene.json", provider);
        ATOM_CHECK(!missing.Ok());
        ATOM_CHECK(HasDiagnostic(missing, "res://prefabs/nope.prefab.json"));

        // A prefab whose own document is invalid: the scene load must fail, not fall
        // back to "an entity with no sprite".
        MapProvider broken{};
        broken.Add("res://prefabs/broken.prefab.json", R"({
            "schema_version": 1, "kind": "prefab", "id": "broken",
            "entity": { "sprite": { "texture": "not-a-uri.png" } }
        })");
        const auto invalid = ParseScene(Scene(R"({ "id": 1, "prefab": "res://prefabs/broken.prefab.json" })"),
                                        "res://scenes/test.scene.json", broken.Provider());
        ATOM_CHECK(!invalid.Ok());
        ATOM_CHECK(HasDiagnostic(invalid, "res://"));
    }

    // --- 8. Prefab documents have their own rules -------------------------
    {
        // `kind` distinguishes a prefab from a scene, so passing the wrong file says so
        // instead of producing unknown-field noise.
        const auto wrong_kind = ParsePrefab(R"({
            "schema_version": 1, "kind": "scene", "id": "x", "entity": {}
        })",
                                            "res://prefabs/x.prefab.json");
        ATOM_CHECK(!wrong_kind.Ok());
        ATOM_CHECK(wrong_kind.diagnostics.front().Format().find("expected \"prefab\"") != std::string::npos);

        const auto missing_kind = ParsePrefab(R"({ "schema_version": 1, "id": "x", "entity": {} })",
                                              "res://prefabs/x.prefab.json");
        ATOM_CHECK(!missing_kind.Ok());
        ATOM_CHECK(missing_kind.diagnostics.front().Format().find("\"kind\"") != std::string::npos);

        // Version is checked before anything else, same rule as a scene.
        const auto bad_version = ParsePrefab(R"({
            "schema_version": 9, "kind": "prefab", "id": "x", "entity": {}
        })",
                                             "res://prefabs/x.prefab.json");
        ATOM_CHECK(!bad_version.Ok());
        ATOM_CHECK(bad_version.diagnostics.front().Format().find("unsupported schema_version 9") !=
                   std::string::npos);

        // Nesting is v1's rejected case; it is also why cycles cannot exist.
        const auto nested = ParsePrefab(R"({
            "schema_version": 1, "kind": "prefab", "id": "outer",
            "entity": { "prefab": "res://prefabs/inner.prefab.json" }
        })",
                                        "res://prefabs/outer.prefab.json");
        ATOM_CHECK(!nested.Ok());
        ATOM_CHECK(nested.diagnostics.front().Format().find("unknown field \"prefab\"") != std::string::npos);

        // A prefab body cannot claim an id: identity comes from the instance.
        const auto with_id = ParsePrefab(R"({
            "schema_version": 1, "kind": "prefab", "id": "x", "entity": { "id": 7 }
        })",
                                         "res://prefabs/x.prefab.json");
        ATOM_CHECK(!with_id.Ok());
        ATOM_CHECK(with_id.diagnostics.front().Format().find("unknown field \"id\"") != std::string::npos);

        // And the body itself is validated with the same rules as a scene entity.
        const auto bad_sprite = ParsePrefab(R"({
            "schema_version": 1, "kind": "prefab", "id": "x",
            "entity": { "sprite": { "texture": "res://textures/x.png", "size": [0, 8] } }
        })",
                                            "res://prefabs/x.prefab.json");
        ATOM_CHECK(!bad_sprite.Ok());
        ATOM_CHECK(bad_sprite.diagnostics.front().Format().find("greater than zero") != std::string::npos);
    }

    return 0;
}

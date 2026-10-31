# ADR-0001: Authoritative game state behind command interfaces

Status: accepted under the user's delegated engineering authority.

## Decision

Combat and expedition rules own serializable state with stable identities. Callers submit validated commands and observe snapshots/results. Actors, cameras, input, and UI adapt that state into Unreal presentation; their lifetime, selection, meshes, and transforms do not decide rules. The existing runtime build module contains cohesive domain and presentation folders.

## Trade-off

Putting rules directly on actors is convenient for a small arena, but couples save/resume, generation, and tests to a loaded world and actor lifetimes. Explicit state introduces some mapping code in presentation, while making deterministic rule tests, save validation, replay, and future presentation changes local. This is a data-ownership decision that would be costly to change after content and saves accumulate.

## Consequences

Combat is exercised through its public command/snapshot seam; expedition transitions and persistence use the public run seam. Rendering and input still require real editor/packaged observations. Use concrete Unreal adapters where needed, without speculative network, plugin, or generic workflow abstractions. Reflected strong UObject members use UPROPERTY-tracked references. Content can be data driven without allowing presentation to bypass command validation.

#include "Presentation/DiceRollTheme.h"
#include "Engine/StaticMesh.h"

UStaticMesh* UDiceRollTheme::Load(const TSoftObjectPtr<UStaticMesh>& Mesh)
{
    return Mesh.IsNull() ? nullptr : Mesh.LoadSynchronous();
}

#include "Rules/LoreRules.h"
#include "Rules/ProficiencyRules.h"

ESkill FLoreRules::RecallSkill(const FString& Profile)
{
    if (Profile == TEXT("Skeleton") || Profile == TEXT("Zombie")) { return ESkill::Religion; }
    if (Profile.Contains(TEXT("Mephit")) || Profile.Contains(TEXT("Elemental"))) { return ESkill::Arcana; }
    if (Profile == TEXT("Wolf") || Profile == TEXT("Boar") || Profile == TEXT("Giant Rat") || Profile == TEXT("Brown Bear") || Profile == TEXT("Giant Spider")) { return ESkill::Nature; }
    return ESkill::History;
}

int32 FLoreRules::RecallDc(const FString& Profile)
{
    return RecallSkill(Profile) == ESkill::Arcana ? 15 : 10;
}

int32 FLoreRules::FactsRevealed(int32 Total, int32 Dc)
{
    return Total < Dc ? 0 : FMath::Min(3, 1 + (Total - Dc) / 5);
}

ESkill FLoreRules::IdentifySkill(bool bMagical) { return bMagical ? ESkill::Arcana : ESkill::History; }
int32 FLoreRules::IdentifyDc(bool bMagical) { return bMagical ? 15 : 10; }

int32 FLoreRules::TreatmentHitPoints(int32 Total, int32 PatientLevel)
{
    return Total >= 10 ? FMath::Max(PatientLevel, 1) : 0;
}

bool FLoreRules::ReadsIntent(int32 InsightTotal, int32 NpcDeceptionModifier)
{
    return InsightTotal >= 10 + NpcDeceptionModifier;
}

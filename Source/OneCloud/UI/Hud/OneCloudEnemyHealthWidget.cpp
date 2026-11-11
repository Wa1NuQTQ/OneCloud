#include "OneCloudEnemyHealthWidget.h"
#include "Components/ProgressBar.h"

void UOneCloudEnemyHealthWidget::SetHealthFraction(float Fraction)
{
	Health->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));
}

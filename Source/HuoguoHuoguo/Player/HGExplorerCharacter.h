#pragma once

#include "CoreMinimal.h"
#include "World/Planet/HGPlanetTypes.h"
#include "Player/HGBossCharacter.h"
#include "HGExplorerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;

/**
 * 탐험 파트의 사장님. 3인칭 카메라로 행성 위를 걷는다.
 *
 * 카메라 구조:
 *   스프링 암이 컨트롤러 회전(마우스 시점)을 따라 돌고, 캐릭터 몸은 이동 방향으로 돈다.
 *   그래서 카메라를 돌려도 캐릭터가 따라 돌지 않고, 걷는 쪽으로만 몸을 튼다. 일반적인 액션 어드벤처식 3인칭이다.
 *   경영 카메라와 달리 시야각을 넓게(약 75도) 써서 가까운 거리감과 속도감을 살린다.
 *
 * 구형 중력:
 *   SetPlanetGravity를 켜면 매 프레임 중력 방향을 "내 위치 -> 행성 중심"으로 바꾼다.
 *   캐릭터 이동 컴포넌트(UE 5.4부터)가 임의 중력 방향을 지원하므로, 방향만 바꿔 주면 걷기, 점프, 경사 판정이
 *   그 방향 기준으로 동작한다. 몸이 기울어진 지면에 맞춰 서는 것도 이동 컴포넌트가 처리한다.
 *
 * 점프 (행성과 소행성 지대 공통, 플랫폼 게임의 손맛):
 *   - 누르는 즉시 뛴다. 길게 누르면(최대 JumpMaxHoldTime) 더 높이, 일찍 떼면 짧게 끊긴다(오를 때 중력 ReleaseGravityScale배).
 *   - 내려올 때는 중력을 FallGravityScale배로 무겁게 해서 붕 떠 있는 느낌 대신 탁 떨어지는 맛을 낸다.
 *   - 착지하고 ChainWindow초 안에 달리면서 다시 뛰면 2단, 3단으로 높아진다. 3단은 공중제비 한 바퀴와 시야 넓히기.
 *   - 너그러운 판정: 발이 떨어진 뒤 CoyoteTime초 안의 점프도 받아 주고(코요테 타임),
 *     착지 직전(발밑 BufferHeight 안)에 누른 점프는 착지하자마자 낸다(입력 미리 받기).
 *   - 눌림과 늘어남: 뛸 때 몸이 위로 늘어나고, 착지할 때 떨어진 속도만큼 눌렸다가 스프링처럼 돌아온다. 착지하면 복셀 먼지가 퍼진다.
 *
 * 소행성 지대 (여러 천체 중력, SetGravityBodies):
 *   - 땅에 서 있으면 밟고 있는 소행성(지표가 가장 가까운 것)에 붙는다.
 *   - 공중에서는 가장 세게 끄는 소행성 쪽으로 중력이 넘어간다. 방향은 초당 GravityTurnRate도까지만 돌아서 몸이 부드럽게 뒤집힌다.
 *     발사대 같은 정해진 길은 없다. 점프와 제트팩만으로 어느 소행성이든 자유롭게 건너간다 (사용자 결정).
 *   - 공중에서 점프를 한 번 더 누르면 제트팩 짧은 분사 (점프 한 번에 한 번, 산소 소모).
 *   - 점프를 놓쳐 오래 떠 있거나 멀리 나가면 산소를 잃고, 가장 가까운 소행성이 세게 끌어 되돌린다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGExplorerCharacter : public AHGBossCharacter
{
	GENERATED_BODY()

public:
	AHGExplorerCharacter();

	// 시점 입력. X = 좌우 회전, Y = 상하 회전.
	void Look(const FVector2D& Input);

	// 카메라 거리 입력 (마우스 휠 한 칸 = ±1). +면 가까이. 한 칸에 ZoomStep 비율만큼 바뀌고 MinArmLength ~ MaxArmLength로 자른다.
	// 실제 거리는 목표 거리로 부드럽게 따라간다 (UpdateJuice).
	void AddZoomInput(float Value);

	void SetPlanetGravity(const FVector& Center, float GravityScale);

	// 소행성 지대: 중력 천체 여러 개. 0번은 처음 서 있는 천체. 둘 이상이면 뛰어 건너기 모드가 켜진다.
	void SetGravityBodies(const TArray<FHGGravityBody>& InBodies);

	// 폐우주선 바깥: 무중력 제트팩 비행. Up = 시점의 기준 "위"(우주선의 위). WASD는 카메라 방향으로 분사, Space/Ctrl은 위/아래 분사,
	// 손을 떼도 관성으로 흐른다(아주 약한 감속). Shift = 브레이크. 분사하는 동안 산소를 쓴다.
	void SetFloating(const FVector& Up);

	// 폐우주선 안: 방향이 고정된 인공 중력 (Up의 반대쪽으로 Scale배). 걷기와 점프는 행성과 같다.
	void SetFixedGravity(const FVector& Up, float Scale);

	bool IsFloating() const { return bFloating; }

	// 무중력 비행 이동 입력 (X = 좌우, Y = 앞뒤, 카메라 방향 그대로 3차원). 컨트롤러가 떠 있을 때 Move 대신 부른다.
	void FloatMove(const FVector2D& Input);

	// 무중력에서 아래 분사(Ctrl), 브레이크(Shift) 누르고 있음.
	void SetDownHeld(bool bHeld) { bDownHeld = bHeld; }
	void SetBrakeHeld(bool bHeld) { bBrakeHeld = bHeld; }

	// 브레이크 세기(1/초). 브레이크는 속도를 매초 exp(-세기)배로 줄이므로, 속도 v에서 멈출 때까지 가는 거리는 약 v / 세기다.
	float GetBrakeStrength() const { return BrakeStrength; }

	bool HasPlanetGravity() const { return bPlanetGravity; }

	// 뛰어 건너기 모드인가 (중력 천체가 둘 이상).
	bool IsHopMode() const { return Bodies.Num() > 1; }

	/**
	 * 중력 기준 좌표계. 이 회전을 적용하면 "중력 기준의 위(Z)"가 월드에서 지표 바깥 방향이 된다.
	 *
	 * 매 프레임 바뀐 만큼만 이어서 돌리기 때문에(평행 이동) 행성 어디를 걸어도 기준이 갑자기 뒤집히지 않는다.
	 * "아래 방향에서 바로 계산"하면 행성 반대편(남극)에서 기준 축이 180도 튀는 특이점이 생긴다.
	 * 중력이 없으면(평면 맵) 항등 회전이다.
	 */
	FQuat GetGravityFrame() const { return GravityFrame; }

	// 이동 입력의 "수평"도 중력 기준으로 잡는다.
	virtual FQuat GetMoveFrame() const override { return GravityFrame; }

	virtual void Tick(float DeltaSeconds) override;

	// 점프 버튼을 눌렀다/뗐다. 땅(또는 코요테 타임): 점프 / 착지 직전: 미리 받기 / 공중(소행성 지대): 제트팩.
	void JumpPressed();
	void JumpReleased();

	// 마지막 점프 단계 (1 ~ 3).
	int32 GetJumpStage() const { return JumpStage; }

	bool IsJetpackReady() const { return bJetpackReady; }

	float GetOxygen() const { return Oxygen; }
	void RefillOxygen(float Amount) { Oxygen = FMath::Clamp(Oxygen + Amount, 0.f, 100.f); }

	// 지금 서 있거나 끌리는 천체 번호 (Bodies 순서).
	int32 GetCurrentBody() const { return CurrentBody; }

	// 쌓인 안내 문구를 꺼낸다 (제트팩, 궤도 이탈, 3단 점프 등). 컨트롤러가 매 프레임 꺼내 화면에 띄운다.
	FString ConsumeHopMessage();

protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float FieldOfView = 75.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ArmLength = 420.f;

	// 마우스 감도 배율.
	UPROPERTY(EditAnywhere, Category = "Camera")
	float LookSensitivity = 1.f;

	// 휠로 바꿀 수 있는 카메라 거리 범위(cm)와 한 칸에 바뀌는 비율 (0.12 = 12%). 비율로 바꿔서 가까울 때는 잘게, 멀 때는 크게 움직인다.
	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinArmLength = 180.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxArmLength = 1600.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ZoomStep = 0.12f;

	// 목표 거리를 따라가는 빠르기 (클수록 빨리).
	UPROPERTY(EditAnywhere, Category = "Camera")
	float ZoomSpeed = 10.f;

	// 착지 먼지 (월드 좌표 인스턴스).
	UPROPERTY(VisibleAnywhere, Category = "Jump")
	TObjectPtr<UInstancedStaticMeshComponent> Dust;

	// 1단 점프 속도(cm/s). 중력 0.4(소행성)에서 짧게 누르면 약 3m, 길게 누르면 약 4m 뜬다.
	UPROPERTY(EditAnywhere, Category = "Jump")
	float BaseJumpVelocity = 520.f;

	// 단계별 점프 속도 배율 (1단, 2단, 3단).
	UPROPERTY(EditAnywhere, Category = "Jump")
	FVector JumpStageScale = FVector(1.f, 1.2f, 1.5f);

	// 착지 뒤 이 시간(초) 안에 다시 뛰어야 다음 단계로 이어진다.
	UPROPERTY(EditAnywhere, Category = "Jump")
	float ChainWindow = 0.3f;

	// 다음 단계로 이으려면 이 속도(cm/s) 이상으로 달리고 있어야 한다 (제자리 연타로는 3단이 안 된다).
	UPROPERTY(EditAnywhere, Category = "Jump")
	float ChainMinSpeed = 250.f;

	// 내려올 때 중력 배율, 오르는 중에 버튼을 떼었을 때 중력 배율.
	UPROPERTY(EditAnywhere, Category = "Jump")
	float FallGravityScale = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Jump")
	float ReleaseGravityScale = 2.f;

	// 코요테 타임(초): 발이 떨어진 뒤에도 점프를 받아 주는 시간.
	UPROPERTY(EditAnywhere, Category = "Jump")
	float CoyoteTime = 0.12f;

	// 입력 미리 받기: 발밑 이 높이(cm) 안에서 내려오는 중에 누른 점프는 착지하자마자 낸다.
	UPROPERTY(EditAnywhere, Category = "Jump")
	float BufferHeight = 120.f;

	// 3단 점프 공중제비 시간(초)과 시야 넓히기(도).
	UPROPERTY(EditAnywhere, Category = "Jump")
	float FlipSeconds = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Jump")
	float FlipFovKick = 8.f;

	// 공중에서 중력 방향이 돌아가는 최대 속도(도/초). 너무 빠르면 몸이 휙 뒤집혀 어지럽고, 느리면 착지할 때 비스듬히 떨어진다.
	UPROPERTY(EditAnywhere, Category = "Hop")
	float GravityTurnRate = 220.f;

	// 아무리 멀어도 이만큼은 끈다 (중력 배율). 우주로 영영 떠나지 않게 하는 바닥값.
	UPROPERTY(EditAnywhere, Category = "Hop")
	float MinPull = 0.15f;

	// 제트팩 분사: 카메라가 보는 쪽으로 더하는 속도(cm/s)와 산소 소모.
	UPROPERTY(EditAnywhere, Category = "Hop")
	float JetpackSpeed = 550.f;

	UPROPERTY(EditAnywhere, Category = "Hop")
	float JetpackOxygen = 3.f;

	// 놓친 점프 판정: 이만큼 오래 떠 있거나(초) 가장 가까운 지표에서 이만큼 멀어지면(cm) 산소를 잃는다.
	UPROPERTY(EditAnywhere, Category = "Hop")
	float MissSeconds = 4.f;

	UPROPERTY(EditAnywhere, Category = "Hop")
	float MissDistance = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Hop")
	float MissOxygen = 10.f;

	// 가장 가까운 지표에서 이보다 멀어지면 그 소행성이 지표 중력으로 세게 끌어 되돌린다(cm).
	UPROPERTY(EditAnywhere, Category = "Hop")
	float RescueDistance = 3500.f;

	// 무중력 비행: 분사 가속(cm/s²), 최고 속도(cm/s), 손을 뗐을 때 감속(cm/s², 작을수록 오래 흐른다), 브레이크 세기(1/초), 분사 중 산소 소모(초당).
	UPROPERTY(EditAnywhere, Category = "Float")
	float FloatAcceleration = 450.f;

	UPROPERTY(EditAnywhere, Category = "Float")
	float FloatMaxSpeed = 700.f;

	UPROPERTY(EditAnywhere, Category = "Float")
	float FloatDrift = 40.f;

	UPROPERTY(EditAnywhere, Category = "Float")
	float BrakeStrength = 3.5f;

	UPROPERTY(EditAnywhere, Category = "Float")
	float ThrustOxygenPerSecond = 0.5f;

private:
	bool bPlanetGravity = false;
	FVector PlanetCenter = FVector::ZeroVector;
	float PlanetGravityScale = 1.f;

	// 방향 고정 중력(우주선 안)이나 무중력(우주선 밖)일 때의 "위".
	bool bFixedUp = false;
	FVector FixedUp = FVector::UpVector;
	bool bFloating = false;
	bool bDownHeld = false;
	bool bBrakeHeld = false;
	bool bThrusting = false;
	float WalkAcceleration = 2048.f; // 걷기 가속 (무중력에서 바꿨다가 되돌린다)
	FQuat GravityFrame = FQuat::Identity;

	TArray<FHGGravityBody> Bodies;
	int32 CurrentBody = 0;

	// 점프 상태
	int32 JumpStage = 1;
	float LastLandTime = -100.f;
	float LastGroundedTime = -100.f;
	bool bJumpedSinceGround = false;
	bool bBufferedJump = false;
	bool bJumpHeld = false;
	float LastFallSpeed = 0.f;

	// 휠로 정한 목표 카메라 거리.
	float DesiredArmLength = 420.f;

	// 손맛 연출: 눌림/늘어남 크기(1이 보통), 공중제비 경과 시간(음수면 꺼짐), 착지 먼지.
	FVector Squash = FVector::OneVector;
	float FlipTime = -1.f;
	float DustTime = -1.f;
	FVector DustCenter = FVector::ZeroVector;
	FQuat DustFrame = FQuat::Identity;

	bool bJetpackReady = false;
	bool bHopAirborne = false;
	float AirTime = 0.f;
	bool bMissCounted = false;
	float Oxygen = 100.f;
	FString HopMessage;

	void UpdatePlanetGravity(float DeltaSeconds);

	// 위치 Pos에서 끄는 천체를 고른다. 땅에 서 있으면 지표가 가장 가까운 천체, 공중이면 가장 세게 끄는 천체.
	// OutScale = 그 천체가 끄는 힘(중력 배율, 바닥값과 되돌리기 포함), OutSurfaceDistance = 가장 가까운 지표까지 거리.
	int32 PickBody(const FVector& Pos, bool bGrounded, float& OutScale, float& OutSurfaceDistance) const;

	// 오르기/내려오기/버튼 뗌에 따른 중력 배율 (1 = 그대로).
	float JumpGravityMultiplier() const;

	// 땅 점프 하나 (연속 점프 단계 계산, 늘어남, 3단 공중제비). bCoyote면 이미 떨어지는 중이라 속도를 직접 넣는다.
	void DoGroundJump(bool bCoyote);

	// 발밑 Height 안에 땅이 있는가 (중력 방향으로 선 검사).
	bool IsGroundWithin(float Height) const;

	// 눌림/늘어남, 공중제비, 시야, 착지 먼지를 매 프레임 갱신한다.
	void UpdateJuice(float DeltaSeconds);

	// 공중에 떴다고 기록한다 (놓친 점프 판정, 제트팩 준비).
	void MarkAirborne();
};

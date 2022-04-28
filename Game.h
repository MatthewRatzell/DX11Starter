#pragma once
#include "Mesh.h"
#include "Camera.h"
#include "GameEntity.h"
#include "DXCore.h"
#include <memory>
#include <vector>
#include "SimpleShader.h"
#include "Material.h"
#include "Lights.h"
#include "Sky.h"
class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void initImGui();
	void makeImGui(float dt);
	void SetUpLightUI(Light& light, int index);
	void SetUpEntityUI(GameEntity* gameEntity, int index);
	void DrawLight();
	void PreRender();
	void PostRender();
	void ResizePostProcessResources();
	void CreatePostProcessSamplerState();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	//creating our 3 meshes for our shapes
	
	

private:
	std::vector<GameEntity*> listOfEntitys;
	//entity
	//shapes and meshes
	std::shared_ptr<Mesh> sphere;
	std::shared_ptr<Mesh> torus;
	std::shared_ptr<Mesh> cube;
	std::shared_ptr<Mesh> cylinder;
	std::shared_ptr<Mesh> helix;
	std::shared_ptr<Mesh> quad;
	//transform
	Transform transform;
	//camera
	std::shared_ptr<Camera> camera;
	//shaders
	std::shared_ptr<SimplePixelShader> pixelShader;
	std::shared_ptr<SimplePixelShader> pixelShader2;
	std::shared_ptr<SimpleVertexShader> vertexShader;

	std::shared_ptr<SimpleVertexShader> vertexShaderNM;
	std::shared_ptr<SimplePixelShader> pixelShaderNM;

	std::shared_ptr<SimpleVertexShader> vertexShaderSky;
	std::shared_ptr<SimplePixelShader> pixelShaderSky;

	std::shared_ptr<SimpleVertexShader> toonVertexShader;
	std::shared_ptr<SimplePixelShader> toonPixelShader;

	//materials
	std::shared_ptr<Material> mat1;
	std::shared_ptr<Material> mat2;
	std::shared_ptr<Material> mat3;
	std::shared_ptr<Material> mat4;
	std::shared_ptr<Material> mat5;
	std::shared_ptr<Material> matSky;

	std::shared_ptr<Material> grassMat;
	std::shared_ptr<Material> cactusMat;
	std::shared_ptr<Material> groundMat;
	std::shared_ptr<Material> rockMat;
	std::shared_ptr<Material> rockMatTwo;
	std::shared_ptr<Material> woodMat;

	//lights and light data
	XMFLOAT3 ambientColor;
	std::vector<Light> lights;
	//sky
	std::shared_ptr<Sky> skyObj;
	// Should we use vsync to limit the frame rate?
	bool vsync;
	float offset;
	//for post processing
	// General post processing resources
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ppRTV;		// Allows us to render to a texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ppSRV;		// Allows us to sample from the same texture

	// Outline rendering --------------------------
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;
	// Initialization helper methods - feel free to customize, combine, etc.
	void LoadShaders(); 
	void CreateBasicGeometry();
	void LoadLights();
	void LoadTexturesSRVsAndSampler();
	void CreateEntitys();
	void OnResize();
	

	


};


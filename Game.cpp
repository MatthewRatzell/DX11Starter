#include "Game.h"
#include <iostream>
#include "DXCore.h"
#include "Vertex.h"
#include "Input.h"
#include "d3dcompiler.h"
#include "BufferStructs.h"
#include "GameEntity.h"
#include "Material.h"
#include "WICTextureLoader.h"
#include "DDSTextureLoader.h"
// Assumes files are in "imgui" subfolder!
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
// For the DirectX Math library
using namespace DirectX;
// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true),			   // Show extra stats (fps) in title bar?
	//call transform constructor
	transform(),
	vsync(false)
{
#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}
// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object created in Game
	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	//make sure we offload our entities in our constructor
	for (auto& e : listOfEntitys) { delete e; }


}
// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{

	//gui
	initImGui();
	//load our shaders and connect them to their correct files
	LoadShaders();

	//set up what we need for our post process
	ResizePostProcessResources();
	CreatePostProcessSamplerState();

	//this is where we set up our global shapes so square 
	CreateBasicGeometry();

	//Run our method that creates all the texture data our shaders will need also making sky here
	LoadTexturesSRVsAndSampler();

	//function that creates all of our entitys
	CreateEntitys();

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//create our camera
	camera = std::make_shared<Camera>(0.0f, 0.0f, -0.0f, (float)width / height);

	LoadLights();

}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{

	makeImGui(deltaTime);
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

	//make sure we update our camera
	camera->Update(deltaTime);
}
// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	//make sure the first thing we do is prerender which will take  care of clearing our render states and buffer
	PreRender();
	///////////////////////////////////////////////////////////////////////////////
	/////////////////////////////Baic shader///////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	offset += .00001f;
	pixelShader->SetFloat("scale", offset);
	pixelShader->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
	///////////////////////////////////////////////////////////////////////////////
	/////////////////////////////Normals///////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	pixelShader2->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////

	//
	//Give the toon pixel shader lights
	toonPixelShader->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());

	/*
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(
		depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);
		*/

		//loop through and draw our entitys
	for (int i = 0; i < listOfEntitys.size(); i++) {
		//going to pass this jawn over to our shader here because for some reason this doesnt belong in entity class but wouldnt it make more sense to pass the ambient color into the entity instead of creating a seperation of tasks that just doesnt make a whole lot of sense, Yeah i get it, this is probably a little less cpu power but im not sure if its worth the loss in coesive code
		listOfEntitys[i]->GetMaterial()->GetPixelShader()->SetFloat3("ambient", ambientColor);
		listOfEntitys[i]->GetMaterial()->BindTexturesAndSamplers();

		listOfEntitys[i]->Draw(context, camera);
	}
	//draw sky here
	{
		skyObj->Draw(context, camera);
	}


	//now that everything is done we can do our postprocessing
	PostRender();
	// Draw ImGui
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(vsync ? 1 : 0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////Helper Functions///////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Game::initImGui()
{
	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Pick a style (uncomment one of these 3)
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();
	//ImGui::StyleColorsClassic();
	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());

}
void Game::LoadShaders()
{
	vertexShader = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(L"VertexShader.cso").c_str());
	pixelShader = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"PixelShader.cso").c_str());

	vertexShaderSky = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(L"skyVS.cso").c_str());
	pixelShaderSky = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"skyPS.cso").c_str());

	pixelShader2 = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"CustomPS.cso").c_str());
	vertexShaderNM = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(L"VertexShaderNM.cso").c_str());

	toonPixelShader = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"ToonShadingPS.cso").c_str());
	toonVertexShader = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(L"VertexShaderNM.cso").c_str());

}
// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	// Create some temporary variables to represent colors
	// - Not necessary, just makes things more readable
	sphere = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/sphere.obj").c_str(), device, context);
	torus = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/torus.obj").c_str(), device, context);
	cube = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/cube.obj").c_str(), device, context);
	cylinder = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/cylinder.obj").c_str(), device, context);
	helix = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/helix.obj").c_str(), device, context);
	quad = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/quad.obj").c_str(), device, context);


}
void Game::LoadTexturesSRVsAndSampler()
{

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create a sampler state for texture sampling options
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());
	//
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler2;
	device->CreateSamplerState(&sampDesc, sampler2.GetAddressOf());

	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler3;
	device->CreateSamplerState(&sampDesc, sampler3.GetAddressOf());

	//skymap
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> skyMap;

	//PBRS
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodMetal;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodNormals;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodRoughness;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedMetal;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedNormals;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedRoughness;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeMetal;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeNormals;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeRoughness;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorMetal;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorNormals;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorRoughness;

	//toon shading
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> rampTexture;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cactusAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cactusNormals;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> grassAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> grassNormals;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> rockAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> rockNormals;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> rockTwoAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> rockTwoNormals;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> groundAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> groundNormals;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodToonAlbedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodToonNormals;


	//setting the sky SRV to its texture
	CreateDDSTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/BrightSky.dds").c_str(), 0, skyMap.GetAddressOf());
	//setting PBR SRVs to their textures
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/wood_albedo.png").c_str(), 0, woodAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/wood_metal.png").c_str(), 0, woodMetal.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/wood_normals.png").c_str(), 0, woodNormals.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/wood_roughness.png").c_str(), 0, woodRoughness.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/scratched_albedo.png").c_str(), 0, scratchedAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/scratched_metal.png").c_str(), 0, scratchedMetal.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/scratched_normals.png").c_str(), 0, scratchedNormals.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/scratched_roughness.png").c_str(), 0, scratchedRoughness.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/bronze_albedo.png").c_str(), 0, bronzeAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/bronze_metal.png").c_str(), 0, bronzeMetal.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/bronze_normals.png").c_str(), 0, bronzeNormals.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/bronze_roughness.png").c_str(), 0, bronzeRoughness.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/floor_albedo.png").c_str(), 0, floorAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/floor_metal.png").c_str(), 0, floorMetal.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/floor_normals.png").c_str(), 0, floorNormals.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/PBR/floor_roughness.png").c_str(), 0, floorRoughness.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/RampTexture.png").c_str(), 0, rampTexture.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/GrassTexture.png").c_str(), 0, grassAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/GrassTexture.png").c_str(), 0, grassNormals.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/RockTexture.png").c_str(), 0, rockAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/RockTexture.png").c_str(), 0, rockNormals.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/RockTextureTwo.png").c_str(), 0, rockTwoAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/RockTextureTwo.png").c_str(), 0, rockTwoNormals.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/GroundTexture.png").c_str(), 0, groundAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/GroundTexture.png").c_str(), 0, groundNormals.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/CactusTexture.png").c_str(), 0, cactusAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/CactusTexture.png").c_str(), 0, cactusNormals.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/WoodTexture.png").c_str(), 0, woodToonAlbedo.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/Toon/WoodTexture.png").c_str(), 0, woodToonNormals.GetAddressOf());

	mat1 = std::make_shared<Material>(vertexShader, pixelShader, XMFLOAT3(1, 1, 1), .9f);

	mat2 = std::make_shared<Material>(vertexShaderNM, pixelShader2, XMFLOAT3(1, 1, 1), 1.0f);
	mat3 = std::make_shared<Material>(vertexShaderNM, pixelShader2, XMFLOAT3(1, 1, 1), 1.0f);

	mat4 = std::make_shared<Material>(vertexShaderNM, toonPixelShader, XMFLOAT3(1, 1, 1), 1.0f);
	mat5 = std::make_shared<Material>(vertexShaderNM, pixelShader2, XMFLOAT3(1, 1, 1), 1.0f);

	grassMat = std::make_shared<Material>(vertexShader, toonPixelShader, XMFLOAT3(1, 1, 1), .9f);
	cactusMat = std::make_shared<Material>(vertexShader, toonPixelShader, XMFLOAT3(1, 1, 1), .9f);
	groundMat = std::make_shared<Material>(vertexShader, toonPixelShader, XMFLOAT3(1, 1, 1), .9f);
	rockMat = std::make_shared<Material>(vertexShader, toonPixelShader, XMFLOAT3(1, 1, 1), .9f);
	rockMatTwo = std::make_shared<Material>(vertexShader, toonPixelShader, XMFLOAT3(1, 1, 1), .9f);
	woodMat = std::make_shared<Material>(vertexShader, toonPixelShader, XMFLOAT3(1, 1, 1), 0.9f);

	/*
	//set the resources for this material
	mat1->AddTextureSRV("SurfaceTexture", rock);//rock
	mat1->AddSampler("BasicSampler", sampler);
	*/

	grassMat->AddSampler("BasicSampler", sampler2);
	grassMat->AddSampler("ToonRampSampler", sampler3);
	grassMat->AddTextureSRV("Albedo", grassAlbedo);
	grassMat->AddTextureSRV("NormalMap", grassNormals);
	grassMat->AddTextureSRV("ToonRamp", rampTexture);

	cactusMat->AddSampler("BasicSampler", sampler2);
	cactusMat->AddSampler("ToonRampSampler", sampler3);
	cactusMat->AddTextureSRV("Albedo", cactusAlbedo);
	cactusMat->AddTextureSRV("NormalMap", cactusNormals);
	cactusMat->AddTextureSRV("ToonRamp", rampTexture);

	rockMat->AddSampler("BasicSampler", sampler2);
	rockMat->AddSampler("ToonRampSampler", sampler3);
	rockMat->AddTextureSRV("Albedo", rockAlbedo);
	rockMat->AddTextureSRV("NormalMap", rockNormals);
	rockMat->AddTextureSRV("ToonRamp", rampTexture);

	rockMatTwo->AddSampler("BasicSampler", sampler2);
	rockMatTwo->AddSampler("ToonRampSampler", sampler3);
	rockMatTwo->AddTextureSRV("Albedo", rockTwoAlbedo);
	rockMatTwo->AddTextureSRV("NormalMap", rockTwoNormals);
	rockMatTwo->AddTextureSRV("ToonRamp", rampTexture);

	groundMat->AddSampler("BasicSampler", sampler2);
	groundMat->AddSampler("ToonRampSampler", sampler3);
	groundMat->AddTextureSRV("Albedo", groundAlbedo);
	groundMat->AddTextureSRV("NormalMap", groundNormals);
	groundMat->AddTextureSRV("ToonRamp", rampTexture);

	woodMat->AddSampler("BasicSampler", sampler2);
	woodMat->AddSampler("ToonRampSampler", sampler3);
	woodMat->AddTextureSRV("Albedo", woodToonAlbedo);
	woodMat->AddTextureSRV("NormalMap", woodToonNormals);
	woodMat->AddTextureSRV("ToonRamp", rampTexture);

	//set these up to use our new PBRs
	mat4->AddSampler("BasicSampler", sampler2);
	mat4->AddSampler("ToonRampSampler", sampler3);
	mat4->AddTextureSRV("Albedo", woodAlbedo);
	mat4->AddTextureSRV("NormalMap", woodNormals);
	mat4->AddTextureSRV("RoughnessMap", woodRoughness);
	mat4->AddTextureSRV("ToonRamp", rampTexture);

	//set these up to use our new PBRs
	mat5->AddSampler("BasicSampler", sampler);
	mat5->AddTextureSRV("Albedo", scratchedAlbedo);
	mat5->AddTextureSRV("NormalMap", scratchedNormals);
	mat5->AddTextureSRV("RoughnessMap", scratchedRoughness);
	mat5->AddTextureSRV("MetalnessMap", scratchedMetal);

	//set these up to use our new PBRs
	mat3->AddSampler("BasicSampler", sampler);
	mat3->AddTextureSRV("Albedo", bronzeAlbedo);
	mat3->AddTextureSRV("NormalMap", bronzeNormals);
	mat3->AddTextureSRV("RoughnessMap", bronzeRoughness);
	mat3->AddTextureSRV("MetalnessMap", bronzeMetal);

	//set these up to use our new PBRs
	mat2->AddSampler("BasicSampler", sampler);
	mat2->AddTextureSRV("Albedo", floorAlbedo);
	mat2->AddTextureSRV("NormalMap", floorNormals);
	mat2->AddTextureSRV("RoughnessMap", floorRoughness);
	mat2->AddTextureSRV("MetalnessMap", floorMetal);

	//make sky
	skyObj = std::make_shared<Sky>(device, sampler2, skyMap, cube, vertexShaderSky, pixelShaderSky);
}
void Game::CreateEntitys()
{
	//creating our 5 entitys
	GameEntity* sphereEntity = new GameEntity(sphere.get(), grassMat);
	GameEntity* torusEntity = new GameEntity(torus.get(), cactusMat);
	GameEntity* cubeEntity = new  GameEntity(cube.get(), rockMat);
	GameEntity* cylinderEntity = new GameEntity(cylinder.get(), rockMatTwo);
	GameEntity* helixEntity = new GameEntity(helix.get(), groundMat);
	GameEntity* quadEntity = new GameEntity(quad.get(), groundMat);

	//create the target
	GameEntity* targetFace = new GameEntity(cylinder.get(), woodMat);
	GameEntity* targetLegLeft = new GameEntity(cylinder.get(), woodMat);
	GameEntity* targetLegRight = new GameEntity(cylinder.get(), woodMat);

	//create the "building"
	GameEntity* roof = new GameEntity(cube.get(), woodMat);
	GameEntity* frontLeft = new GameEntity(cube.get(), woodMat);
	GameEntity* frontRight = new GameEntity(cube.get(), woodMat);
	GameEntity* backLeft = new GameEntity(cube.get(), woodMat);
	GameEntity* backRight = new GameEntity(cube.get(), woodMat);

	//create the barrier for the building
	GameEntity* support = new GameEntity(cube.get(), woodMat);
	GameEntity* counter = new GameEntity(cube.get(), woodMat);

	//pushing entitys to list
	listOfEntitys.push_back(sphereEntity);
	listOfEntitys.push_back(torusEntity);
	listOfEntitys.push_back(cubeEntity);
	listOfEntitys.push_back(cylinderEntity);
	listOfEntitys.push_back(helixEntity);
	listOfEntitys.push_back(quadEntity);
	listOfEntitys.push_back(targetFace);
	listOfEntitys.push_back(targetLegLeft);
	listOfEntitys.push_back(targetLegRight);
	listOfEntitys.push_back(roof);
	listOfEntitys.push_back(frontLeft);
	listOfEntitys.push_back(frontRight);
	listOfEntitys.push_back(backLeft);
	listOfEntitys.push_back(backRight);
	listOfEntitys.push_back(support);
	listOfEntitys.push_back(counter);

	//randomly spawn in 100 blades of grass around the map
	for (int i = 0; i < 100; i++)
	{
		GameEntity* grassEntity = new  GameEntity(cube.get(), grassMat);

		//gets a random value between -50 and 50
		int xVal = rand() % 100 - 50;
		int zVal = rand() % 100 - 50;

		grassEntity->GetTransform()->SetPosition(xVal, -0.5, zVal);
		grassEntity->GetTransform()->SetRotation(0, 0, 0);
		grassEntity->GetTransform()->SetScale(0.01, 0.3, 0.01);

		listOfEntitys.push_back(grassEntity);

	}

	//randomly spawn in 50 cacti
	for (int i = 0; i < 50; i++)
	{
		GameEntity* grassEntity = new  GameEntity(sphere.get(), cactusMat);

		//gets a random value between -50 and 50
		int xVal = rand() % 100 - 50;
		int zVal = rand() % 100 - 50;

		int randomNum = rand() % 100;
		float scaleVal = 0.75;

		//add some variation to the cacti
		if (randomNum > 50)
		{
			scaleVal = 1;
		}

		grassEntity->GetTransform()->SetPosition(xVal, 0, zVal);
		grassEntity->GetTransform()->SetRotation(0, 0, 0);
		grassEntity->GetTransform()->SetScale(0.5, scaleVal, 0.5);

		listOfEntitys.push_back(grassEntity);
	}

	//randomly spawn 25 rocks
	for (int i = 0; i < 50; i++)
	{

		int randomNum = rand() % 100;

		GameEntity* rockEntity;
		
		//add some diversity to the rock color
		if (randomNum > 50)
		{
			rockEntity = new GameEntity(cube.get(), rockMat);
		}
		else {
			rockEntity = new GameEntity(cube.get(), rockMatTwo);
		}

		//gets a random value between -50 and 50
		int xVal = rand() % 100 - 50;
		int zVal = rand() % 100 - 50;
		
		int scaleVal = 0.85;

		//add some variation to the cacti
		if (randomNum > 50)
		{
			scaleVal = 1.75;
		}

		rockEntity->GetTransform()->SetPosition(xVal, -0.5, zVal);
		rockEntity->GetTransform()->SetRotation(zVal, (zVal + xVal) / 2, xVal);
		rockEntity->GetTransform()->SetScale(1, 1, 1.25);

		listOfEntitys.push_back(rockEntity);
	}

	/////////////////////////////////
	//making sure we put them in a good spot
	listOfEntitys[0]->GetTransform()->SetPosition(0, -10, 0);
	listOfEntitys[1]->GetTransform()->SetPosition(-2.5, -10, 0);
	listOfEntitys[2]->GetTransform()->SetPosition(2.5, -10, 0);
	listOfEntitys[3]->GetTransform()->SetPosition(-5.5, -10, 0);
	listOfEntitys[4]->GetTransform()->SetPosition(7.5, -10, 0);
	listOfEntitys[5]->GetTransform()->SetPosition(0, -0.5, 0);
	listOfEntitys[5]->GetTransform()->SetScale(100, 0, 100);

	//position the target
	targetFace->GetTransform()->SetPosition(0, 1, 6);
	targetFace->GetTransform()->SetRotation(90, 0, 0);
	targetFace->GetTransform()->SetScale(1, 0.25, 1);

	targetLegLeft->GetTransform()->SetPosition(-0.5, 0, 6);
	targetLegLeft->GetTransform()->SetRotation(0, 0, 0);
	targetLegLeft->GetTransform()->SetScale(0.1, 1, 0.1);

	targetLegRight->GetTransform()->SetPosition(0.5, 0, 6);
	targetLegRight->GetTransform()->SetRotation(0, 0, 0);
	targetLegRight->GetTransform()->SetScale(0.1, 1, 0.1);

	//position the building
	roof->GetTransform()->SetPosition(0, 5, 0);
	roof->GetTransform()->SetScale(5, 0.1, 10);

	frontLeft->GetTransform()->SetPosition(4, 0, 9);
	frontLeft->GetTransform()->SetScale(0.15, 5, 0.15);

	frontRight->GetTransform()->SetPosition(-4, 0, 9);
	frontRight->GetTransform()->SetScale(0.15, 5, 0.15);

	backLeft->GetTransform()->SetPosition(4, 0, -9);
	backLeft->GetTransform()->SetScale(0.15, 5, 0.15);

	backRight->GetTransform()->SetPosition(-4, 0, -9);
	backRight->GetTransform()->SetScale(0.15, 5, 0.15);

	//position the barrier
	support->GetTransform()->SetPosition(0, 0, -7.5);
	support->GetTransform()->SetScale(4, 0.75, 0.25);

	counter->GetTransform()->SetPosition(0, 0.5, -7.5);
	counter->GetTransform()->SetScale(4.5, 0.15, 0.5);

	/////////////////////////////////
}
void Game::LoadLights()
{
	//create our ambient color taht will be sent to  pixel shader
	ambientColor = XMFLOAT3(0.1, 0.1, 0.25);

	Light dirLight1 = {};
	Light dirLight2 = {};
	Light dirLight3 = {};
	Light pointLight1 = {};
	Light pointLight2 = {};
	//setting only the values we need to set for a directional light
	dirLight1.Type = 0;
	dirLight2.Type = 0;
	dirLight3.Type = 0;
	pointLight1.Type = 1;
	pointLight2.Type = 1;
	//pointing right
	dirLight1.Direction = XMFLOAT3(1, -0.5, 1);
	dirLight2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dirLight3.Direction = XMFLOAT3(1, -1, 1);
	/// /////color////////////////
	dirLight1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dirLight2.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dirLight3.Color = XMFLOAT3(0, 0, 1);
	pointLight1.Color = XMFLOAT3(.5, .5, .5);
	pointLight2.Color = XMFLOAT3(1, 1, 1);
	/// //////intensity/////////////
	dirLight1.Intensity = .8;
	dirLight2.Intensity = .51;
	dirLight3.Intensity = .41;
	pointLight1.Intensity = .1;
	pointLight2.Intensity = .90;
	/// //////Position(not for directionals)/////////////
	pointLight1.Position = XMFLOAT3(-7, 3, 0);
	pointLight2.Position = XMFLOAT3(0, -1, 0);
	/// //////Range(not for directionals)/////////////
	pointLight1.Range = 10.0f;
	pointLight2.Range = 5.0f;
	lights.push_back(dirLight1);
	//lights.push_back(dirLight2);
	//lights.push_back(dirLight3);
	//lights.push_back(pointLight1);
	//lights.push_back(pointLight2);
}
void Game::OnResize()
{

	// Handle base-level DX resize stuff
	DXCore::OnResize();
	//make sure we update our projection matrix when the screen resizes
	camera->UpdateProjectionMatrix((float)this->width / this->height);
	ResizePostProcessResources();
}
void Game::SetUpLightUI(Light& light, int index) {

	//create the unique index for this gui object
	std::string indexStr = std::to_string(index);

	//have a name for this light tree
	std::string nodeName = "Light " + indexStr;

	//if the tree is open, or in other words, if this light is chosen, show its options
	if (ImGui::TreeNode(nodeName.c_str()))
	{
		//give these buttons a unique id
		std::string radioDirID = "Directional##" + indexStr;
		std::string radioPointID = "Point##" + indexStr;

		//if this button with this id is choosen
		if (ImGui::RadioButton(radioDirID.c_str(), light.Type == LIGHT_TYPE_DIRECTIONAL))
		{
			//change light type to directional
			light.Type = LIGHT_TYPE_DIRECTIONAL;
		}
		ImGui::SameLine();

		//if this button with this id is chossen
		if (ImGui::RadioButton(radioPointID.c_str(), light.Type == LIGHT_TYPE_POINT))
		{
			//change the light to a point light
			light.Type = LIGHT_TYPE_POINT;
		}
		ImGui::SameLine();

		// Direction
		if (light.Type == LIGHT_TYPE_DIRECTIONAL)
		{
			std::string dirID = "Direction##" + indexStr;

			//create a dragger for 3 floats that auto updates the direction
			ImGui::DragFloat3(dirID.c_str(), &light.Direction.x, 0.1f);

			// Normalize the direction
			XMVECTOR dirNorm = XMVector3Normalize(XMLoadFloat3(&light.Direction));

			//now actually change the lights direction with the newly normalized 3 floats
			XMStoreFloat3(&light.Direction, dirNorm);
		}

		// Position & Range
		if (light.Type == LIGHT_TYPE_POINT)
		{
			//create an id  for the position and create our dragger that lets up auto update the position
			std::string posID = "Position##" + indexStr;
			ImGui::DragFloat3(posID.c_str(), &light.Position.x, 0.1f);

			//create an id  for the range and create our dragger that lets up auto update the range
			std::string rangeID = "Range##" + indexStr;
			ImGui::SliderFloat(rangeID.c_str(), &light.Range, 0.1f, 100.0f);
		}


		/// ///////////////////////////////////////////////////////////////////////////////////////////////////
		/// ////////////////////////////////every light has these optionss//////////////////////////////////////////////
		/// /// ///////////////////////////////////////////////////////////////////////////////////////////////////
		std::string buttonID = "Color##" + indexStr;
		ImGui::ColorEdit3(buttonID.c_str(), &light.Color.x);

		std::string intenseID = "Intensity##" + indexStr;
		ImGui::SliderFloat(intenseID.c_str(), &light.Intensity, 0.0f, 10.0f);

		//close the tree
		ImGui::TreePop();
	}
}
void Game::makeImGui(float dt) {


	//grab the jimmy jawn pizza from pizzi hut 
	Input& input = Input::GetInstance();

	input.SetGuiKeyboardCapture(false);
	input.SetGuiMouseCapture(false);

	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = dt;
	io.DisplaySize.x = (float)this->width;
	io.DisplaySize.y = (float)this->height;
	io.KeyCtrl = input.KeyDown(VK_CONTROL);
	io.KeyShift = input.KeyDown(VK_SHIFT);
	io.KeyAlt = input.KeyDown(VK_MENU);
	io.MousePos.x = (float)input.GetMouseX();
	io.MousePos.y = (float)input.GetMouseY();
	io.MouseDown[0] = input.MouseLeftDown();
	io.MouseDown[1] = input.MouseRightDown();
	io.MouseDown[2] = input.MouseMiddleDown();
	io.MouseWheel = input.GetMouseWheel();
	input.GetKeyArray(io.KeysDown, 256);
	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	// Determine new input capture (youll uncomment later)
	input.SetGuiKeyboardCapture(io.WantCaptureKeyboard);
	input.SetGuiMouseCapture(io.WantCaptureMouse);

	// Combined into a single window
	ImGui::Begin("Debug");

	//make a lights div
	if (ImGui::CollapsingHeader("Lights"))
	{
		//go through the vector of lights and produce the UI and controls for each one
		for (int i = 0; i < lights.size(); i++)
		{
			SetUpLightUI(lights[i], i);
		}
	}

	// All scene entities
	if (ImGui::CollapsingHeader("Entities"))
	{

		for (int i = 0; i < listOfEntitys.size(); i++)
		{
			SetUpEntityUI(listOfEntitys[i], i);
		}
	}

	ImGui::End();

}
void Game::SetUpEntityUI(GameEntity* gameEntity, int index) {


	std::string indexStr = std::to_string(index);

	std::string nodeName = "Entity " + indexStr;

	if (ImGui::TreeNode(nodeName.c_str()))
	{
		// Transform -----------------------
		if (ImGui::CollapsingHeader("Transform"))
		{
			Transform* transform = gameEntity->GetTransform();

			//grab their initial values so we can have our sliders start at the right value 
			XMFLOAT3 pos = transform->GetPosition();
			XMFLOAT3 rot = transform->GetRotation();
			XMFLOAT3 scale = transform->GetScale();

			//create three unique names so that our sliders all have a different name
			std::string posID = "PositionOfEntity##" + indexStr;
			std::string pyrID = "PitchYaWRollOfEntity##" + indexStr;
			std::string scaleID = "ScaleOfEntity##" + indexStr;
			//create position slider
			if (ImGui::DragFloat3(posID.c_str(), &pos.x, 0.1f))
			{
				//make sure we actually set the position because this isnt lical
				transform->SetPosition(pos.x, pos.y, pos.z);
			}

			if (ImGui::DragFloat3(pyrID.c_str(), &rot.x, 0.1f))
			{
				transform->SetRotation(rot.x, rot.y, rot.z);
			}

			if (ImGui::DragFloat3(scaleID.c_str(), &scale.x, 0.1f, 0.0f))
			{
				transform->SetScale(scale.x, scale.y, scale.z);
			}
		}
		ImGui::TreePop();
	}
}
void Game::DrawLight() {



}
//clears our rendered images and our depth stencil  view
void Game::PreRender()
{
	// Background color for clearing
	const float color[4] = { 0, 0, 0, 1 };

	// Clear the render target and depth buffer (erases what's on the screen)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Clear all render targets, too
	context->ClearRenderTargetView(ppRTV.Get(), color);

	// Assume three render targets (since pixel shader is always returning 3 numbers)
	ID3D11RenderTargetView* rtvs[1] =
	{
		backBufferRTV.Get(),
	};


	rtvs[0] = ppRTV.Get();

	// Set all three
	context->OMSetRenderTargets(1, rtvs, depthStencilView.Get());
}
//this is where we can handle all of the post processing at the moment we are only doing sobel filtering
void Game::PostRender()
{
	// Now that the scene is rendered, swap to the back buffer
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);
	std::shared_ptr<SimplePixelShader> sobelFilterPS = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"sobelFilterPS.cso").c_str());
	std::shared_ptr<SimpleVertexShader> fullscreenVS = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(L"fullscreenVS.cso").c_str());
	// Set up post process shaders
	fullscreenVS->SetShader();

	//set all of the info our outlining pixel shader needs as well as passing it  to the pixel shader
	sobelFilterPS->SetShader();
	sobelFilterPS->SetShaderResourceView("pixels", ppSRV.Get());
	sobelFilterPS->SetSamplerState("samplerOptions", clampSampler.Get());
	sobelFilterPS->SetFloat("pixelWidth", 1.0f / width);
	sobelFilterPS->SetFloat("pixelHeight", 1.0f / height);
	sobelFilterPS->CopyAllBufferData();

	// Draw exactly 3 vertices, which the special post-process vertex shader will
	// "figure out" on the fly (resulting in our "full screen triangle")
	context->Draw(3, 0);

	// Unbind shader resource views at the end of the frame,
	// since we'll be rendering into one of those textures
	// at the start of the next
	ID3D11ShaderResourceView* nullSRVs[128] = {};
	context->PSSetShaderResources(0, ARRAYSIZE(nullSRVs), nullSRVs);
}
//need to change this because if our screen resizes the data requirements we have been sending the RTVs are wrong
void Game::ResizePostProcessResources()
{
	// Reset all resources (releasing them)
	ppRTV.Reset();
	ppSRV.Reset();


	// Describe our textures
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Will render to it and sample from it!
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	// Create the color and normals textures
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ppTexture;
	device->CreateTexture2D(&textureDesc, 0, ppTexture.GetAddressOf());


	// Create the Render Target Views (null descriptions use default settings)
	device->CreateRenderTargetView(ppTexture.Get(), 0, ppRTV.GetAddressOf());

	// Create the Shader Resource Views (null descriptions use default settings)
	device->CreateShaderResourceView(ppTexture.Get(), 0, ppSRV.GetAddressOf());

}
//called on start up  to make sure we have the right sampler settings(same as  normal just had to activate clamping)
void Game::CreatePostProcessSamplerState()
{
	// Create a sampler state for texture sampling options
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	// Create a second sampler for with clamp address mode
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	device->CreateSamplerState(&sampDesc, clampSampler.GetAddressOf());
}

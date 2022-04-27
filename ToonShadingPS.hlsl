#include "ShaderIncludes.hlsli" 
#include "Lighting.hlsli"
#define NUM_LIGHTS 3

cbuffer ExternalData : register(b0)
{
	float3 colorTint;
	float3 cameraPosition;
	float3 ambient;
	Light lights[NUM_LIGHTS];

}

Texture2D Albedo : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
Texture2D ToonRamp : register(t3);
SamplerState BasicSampler : register(s0);
SamplerState ToonRampSampler : register(s1);

//=============================================================================
//uvs scaled by 4
float4 main(VertexToPixelNormalMapping input) : SV_TARGET
{
	///////////////////////////////////////////////////////////////////////////////
	////////////////////////normal/////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	input.normal = normalize(input.normal);

//code for including normal maps--

//get our unpacked normals which we get by converting the color
//float3 unpackedNormal = NormalMap.Sample(BasicSampler, input.uv * 3).rgb * 2 - 1;

// Simplifications include not re-normalizing the same vector more than once!
/*float3 N = normalize(input.normal); // Must be normalized here or before
float3 T = normalize(input.tangent); // Must be normalized here or before
T = normalize(T - N * dot(T, N)); // Gram-Schmidt assumes T&N are normalized!
float3 B = cross(T, N);
float3x3 TBN = float3x3(T, B, N); */

//finally produce our correct normals
//input.normal = (normalize(mul(unpackedNormal, TBN))); // Note multiplication order!

/////////////////////////////////////////////////////////////////////////////
//////////////////////////surface///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//get our surfaceColor(texture) albedo
float3 surfaceColor = pow(Albedo.Sample(BasicSampler, input.uv * 3).rgb,2.2);
//multiply our surface color by our colorTint to make sure its blended good 
surfaceColor *= colorTint;
///////////////////////////////////////////////////////////////////////////////
//////////////////////////roughness(used for Cook)////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//do not gamma correct
float rough = RoughnessMap.Sample(BasicSampler, input.uv * 3).r;

///////////////////////////////////////////////////////////////////////////
/////////////////////////specular color(used for Cook)////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
// Specular color determination -----------------
// Assume albedo texture is actually holding specular color where metalness == 1
//
// Note the use of lerp here - metal is generally 0 or 1, but might be in between
// because of linear texture sampling, so we lerp the specular color to match
float3 specularColor = 0;//lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);
////////////////////////////////////////////////////////////////////
/////////////////////////Ambient////////////////////////////////////
////////////////////////////////////////////////////////////////////
float3 lightTotal = ambient * surfaceColor;
////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////Lighting///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

for (int i = 0; i < NUM_LIGHTS; i++)
{
	Light light = lights[i];
	light.Direction = normalize(light.Direction);

	//run the right method for the right light
	switch (lights[i].Type)
	{
	case LIGHT_TYPE_DIRECTIONAL:
		lightTotal += CreateDirectionalLightToon(lights[i], input.normal, rough, surfaceColor, cameraPosition, input.worldPosition,specularColor, ToonRamp, ToonRampSampler);
		break;

	case LIGHT_TYPE_POINT:
		
		lightTotal += CreatePointLightToon(lights[i], input.normal, rough, surfaceColor, cameraPosition, input.worldPosition, specularColor, ToonRamp, ToonRampSampler);
		break;
	}

}

//////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
float3 finalPixelColor = lightTotal;
return float4(pow(finalPixelColor, 1.0f / 2.2f), 1);


}
#include "Header.fx"

//TerrainDatas
struct VertexOutput
{
    float4 Position : SV_POSITION;
    float2 Uv : TEXCOORD0;
    float3 Normal : NORMAL0;
	float3 Tangent : TANGENT0;
};

//BrushDatas
cbuffer BrushDataBuffer
{
	float2 BrushRangeMin;
	float2 BrushRangeMax;
};

cbuffer TimeBuffer
{
	float RunningTime;
};

cbuffer PressBuffer
{
	bool bPress;
};

matrix BrushWorld;
struct Brush_VertexOutput
{
	float4 Position : SV_POSITION;
	float2 Uv : TEXCOORD0;
};

Texture2D BrushTexture;
SamplerState BrushSampler;

float4 tempColor;
float2 tempUv;

float2 CalcBrushUv(float x, float z);

//-----------------------------------------------------------------------------
// ComputeShader
//-----------------------------------------------------------------------------
StructuredBuffer<float3> CSVertexData;
Texture2D CSInputTexture;
RWTexture2D<float3> CSOutputTexture;
SamplerState CSOutputSampler;
Texture2D CSHeightTex; //Test

[numthreads(32, 32, 1)] //스레드그룹 하나의 크기
void CS(int3 id : SV_DispatchThreadId) //한 연산에서의 스레드 아이디
{
	//브러쉬 범위 안의 정점이라면 AdjustBrush 함수 실행
	float2 tempUv = float2(0.0f, 0.0f);

	//브러쉬 범위 판별
	if (CSVertexData[id.x * id.y].x >= BrushRangeMin.x && CSVertexData[id.x * id.y].x <= BrushRangeMax.x)
	{
		if (CSVertexData[id.x * id.y].z >= BrushRangeMin.y && CSVertexData[id.x * id.y].z <= BrushRangeMax.y)
		{
			tempUv = float2((CSVertexData[id.x * id.y].x - BrushRangeMin.x) / 30.0f, 1.0f - ((CSVertexData[id.x * id.y].z - BrushRangeMin.y) / 30.0f));
		}
	}
	
	tempColor = BrushTexture.SampleLevel(BrushSampler, tempUv, 0.0f);

	CSOutputTexture[id.xy] = CSHeightTex[id.xy];
	
	//GroupMemoryBarrierWithGroupSync(); //스레드들이 해당 연산을 다 마칠때 까지 대기
}

//-----------------------------------------------------------------------------
// Terrain_VertexShader<><><><><><><><><><><><><><><><><><><><><><><><><><><><ㅣ
//-----------------------------------------------------------------------------
VertexOutput VS(VertexTextureNormalTangent input)
{
	VertexOutput output;

	////1. 정점 결과를 텍스쳐에 저장
	////2. 동적 버퍼에 저장
	//int index = 256 * (int)input.Position.y + (int)input.Position.x;
	//
	//if (bPress == true)
	//{
	//	tempUv = CalcBrushUv(input.Position.x, input.Position.z);
	//	tempColor = BrushTexture.SampleLevel(BrushSampler, tempUv, 0);
	//
	//	input.Position.y += (tempColor.a * RunningTime * 7.0f);
	//}

	////지면 브러쉬 적용 수정버전
	//float2 BrushUv = float2(input.Position.x / 256.0f, input.Position.z / 256.0f);
	//input.Position.y += CSOutputTexture.SampleLevel(CSOutputSampler, BrushUv, 0).g;
	////

	////CS Test
	//float2 tempUv2 = float2(input.Position.x / 255.0f, input.Position.z / 255.0f);
	////input.Position.y = CSOutputTexture.SampleLevel(CSOutputSampler, tempUv2, 0.0f).g;
	//float4 tempHeight = CSHeightTex.SampleLevel(CSOutputSampler, tempUv2, 0.0f);
	//input.Position.y = tempHeight.y;

	output.Position = mul(input.Position, World);
	output.Position = mul(output.Position, View);
    output.Position = mul(output.Position, Projection);
	
	output.Normal = mul(input.Normal, (float3x3) World);
	output.Uv = input.Uv * 8.0f;

	output.Tangent = mul(input.Tangent, (float3x3) World);
	
    return output;
}

float2 CalcBrushUv(float x, float z)
{
	//브러쉬 범위 안의 정점이라면 AdjustBrush 함수 실행
	float2 tempUv = float2(0.0f, 0.0f);

	if (x >= BrushRangeMin.x && x <= BrushRangeMax.x)
	{
		if (z >= BrushRangeMin.y && z <= BrushRangeMax.y)
		{
			tempUv = float2((x - BrushRangeMin.x) / 30.0f, 1.0f - ((z - BrushRangeMin.y) / 30.0f));
		}
	}

	return tempUv;
}


//-----------------------------------------------------------------------------
// Brush_VertexShader                                                         l
//-----------------------------------------------------------------------------
Brush_VertexOutput Brush_VS(VertexTexture input)
{
	Brush_VertexOutput output;

	output.Position = mul(input.Position, BrushWorld);

	output.Position = mul(output.Position, View);
	output.Position = mul(output.Position, Projection);

	output.Uv = input.Uv;

	return output;
}

//-----------------------------------------------------------------------------
// Brush_PixelShader
//-----------------------------------------------------------------------------
BlendState blendState
{
	AlphaToCoverageEnable = TRUE;

	BlendEnable[0] = TRUE;
	SrcBlend = SRC_COLOR;
	DestBlend = INV_SRC_COLOR;
	//BlendOp = ADD;
	//SrcBlendAlpha = ZERO;
	//DestBlendAlpha = ZERO;
	//BlendOpAlpha = ADD;
	//RenderTargetWriteMask[0] = 0x0F;
};

float4 Brush_PS(Brush_VertexOutput input) : SV_TARGET
{
	float4 color = BrushTexture.Sample(BrushSampler, input.Uv);
	float4 blue = float4(0.51f, 0.6f, 0.9f, 0.0f);

	return color + blue;
}
//-----------------------------------------------------------------------------
// Terrain_PixelShader
//-----------------------------------------------------------------------------
SamplerState Sampler
{
	AddressU = WRAP;
	AddressV = WRAP;
};

SamplerState NormalSampler
{
	AddressU = WRAP;
	AddressV = WRAP;
};

float4 PS(VertexOutput input) : SV_TARGET
{
	float4 color = 0;

	float4 diffuse = DiffuseMap.Sample(Sampler, input.Uv);
	DiffuseLighting(color, diffuse, input.Normal);

	//노멀맵핑
	float4 normal = NormalMap.Sample(NormalSampler, input.Uv * 4.0f);
	NormalMapping(color, normal, input.Normal, input.Tangent);

	//color.a = 1.0f;
    return color;
}

RasterizerState FillMode
{
	FillMode = Wireframe;
};

//-----------------------------------------------------------------------------
// Technique
//-----------------------------------------------------------------------------
technique11 T0
{
    pass P0 //Terrain
    {
		SetComputeShader(CompileShader(cs_5_0, CS()));
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS()));

		//SetRasterizerState(FillMode);
	}
	pass P1 //Brush
	{
		SetVertexShader(CompileShader(vs_5_0, Brush_VS()));
		SetPixelShader(CompileShader(ps_5_0, Brush_PS()));
		
		SetBlendState(blendState, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}

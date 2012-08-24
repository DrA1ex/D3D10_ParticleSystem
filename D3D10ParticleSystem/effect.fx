//--------------------------------------------------------------------------------------
matrix View;
matrix Projection;

float Size;
float Width, Height;
Texture2D Base;

float X,Y,G, Resistance;

SamplerState Filter
{
	Filter = ANISOTROPIC;
};

struct VS_OUT
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
};

//--------------------------------------------------------------------------------------
// StreamOut
//--------------------------------------------------------------------------------------

VS_OUT SO(float4 Pos : SV_Position, float4 vel : COLOR)
{
	VS_OUT Out;

	float force = 0;
	float dx = X - Pos.x, dy = Y - Pos.y;

	float distSquare = pow( dx, 2 ) + pow( dy, 2 );
	if( distSquare >= 400 ) // A magic number represent min process distance
	{
		force = G / distSquare;
	}

	float xForce = dx * force;
	float yForce = dy * force;

	vel.x *= Resistance;
	vel.y *= Resistance;

	vel.x += xForce;
	vel.y += yForce;

	Pos.x+= vel.x;
	Pos.y+= vel.y;

	if( Pos.x > Width )
		Pos.x -= Width;
	else if( Pos.x < 0 )
		Pos.x += Width;

	if( Pos.y > Height )
		Pos.y -= Height;
	else if( Pos.y < 0 )
		Pos.y += Height;

	Out.pos = Pos;
	Out.color = vel;
	return Out;
}

[maxvertexcount(1)]
void StreamOutGS(point VS_OUT In[1], inout PointStream<VS_OUT> Out)
{
	VS_OUT v = In[0];
	Out.Append(v);
}

GeometryShader gsStreamOut = ConstructGSWithSO(
	CompileShader( gs_4_0, StreamOutGS() ),
	"SV_POSITION.xyz; COLOR0.xyz" );

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUT VS( float4 Pos : SV_Position, float4 vel : COLOR )
{
	VS_OUT Out;
	Out.pos = Pos;

	//Out.color = float4(vel.x, vel.y, 1, 1.0f);
	Out.color = float4(1,1,1,1);
	return Out;
}

//--------------------------------------------------------------------------------------
// GEOMETRY SHADER
//--------------------------------------------------------------------------------------
struct GS_OUT
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
	float2 tex : TEXCOORD0;
};

[maxvertexcount(4)]
void GS( point VS_OUT sprite[1], inout TriangleStream<GS_OUT> triStream )
{
	GS_OUT v;
	v.color = sprite[0].color;

	float hStep = Size/2;
	float wStep = Size/2;

	v.pos = sprite[0].pos + float4(-wStep, -hStep, 0, 0);
	v.pos = mul( v.pos, View );
	v.pos = mul( v.pos, Projection );
	v.tex = float2(0,0);
	triStream.Append(v);

	v.pos = sprite[0].pos + float4(wStep, -hStep, 0, 0);
	v.pos = mul( v.pos, View );
	v.pos = mul( v.pos, Projection );
	v.tex = float2(0,1);
	triStream.Append(v);

	v.pos = sprite[0].pos + float4(-wStep, hStep, 0, 0);
	v.pos = mul( v.pos, View );
	v.pos = mul( v.pos, Projection );
	v.tex = float2(1,0);
	triStream.Append(v);

	v.pos = sprite[0].pos + float4(wStep, hStep, 0, 0);
	v.pos = mul( v.pos, View );
	v.pos = mul( v.pos, Projection );
	v.tex = float2(1,1);
	triStream.Append(v);

}
//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( GS_OUT In ) : SV_TARGET
{
	return In.color*Base.Sample(Filter, In.tex);
}


//--------------------------------------------------------------------------------------
technique10 Render
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( CompileShader( gs_4_0, GS() ) );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
	}
}

technique10 StreamOut
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, SO() ) );
		SetGeometryShader( gsStreamOut  );
		SetPixelShader(  NULL );
	}
}
Texture2D shaderTexture : register(t0);

SamplerState SampleType;

struct PixelInputType {
	float4 position : SV_POSITION;
    float2 tex      : TEXCOORD;
    float3 PosW     : POSITION;
    float3 NormalW  : NORMAL;
    float3 TangentW : TANGENT;
    float4 Color    : COLOR;
};

static const float2 poisson[12] = {
        float2(-0.326212f, -0.40581f),
        float2(-0.840144f, -0.07358f),
        float2(-0.695914f, 0.457137f),
        float2(-0.203345f, 0.620716f),
        float2(0.96234f, -0.194983f),
        float2(0.473434f, -0.480026f),
        float2(0.519456f, 0.767022f),
        float2(0.185461f, -0.893124f),
        float2(0.507431f, 0.064425f),
        float2(0.89642f, 0.412458f),
        float2(-0.32194f, -0.932615f),
        float2(-0.791559f, -0.59771f)
};

float4 PixelShaderFX(PixelInputType input) : SV_TARGET {

	float4 cOut = 0;
	//float2 Center = float2(0.5f, 0.5f);
	//input.tex -= Center;

	//float BlurAmount = 0.1f;
	//for (int i = 0; i < 15; i++) {
	//	float scale = 1.0 + BlurAmount * (i / 14.0);
	//	cOut += shaderTexture.Sample(SampleType, input.tex * scale + Center);
	//}
   
	//cOut /= 15;
    
    float Threshold = 0.9f;
    // Look up the original image color.
    float4 originalColor = shaderTexture.Sample(SampleType, input.tex);
    
	float DiskRadius = 5.0f;
	float2 InputSize = float2(800.0f, 800.0f);
	// Center tap
	float3 rgb = originalColor.rgb;
    rgb = saturate((rgb - Threshold) / (1 - Threshold));
	rgb.r *= 0.1f;
	rgb.b *= 0.88f;
	cOut = float4(rgb, originalColor.a);
	for (int tap = 0; tap < 12; tap++) {
		float2 coord = input.tex.xy + (poisson[tap] / InputSize * DiskRadius);
		float3 rgb2 = shaderTexture.Sample(SampleType, coord).rgb;
		rgb2 = saturate((rgb2 - Threshold) / (1 - Threshold));
		rgb2.r *= 0.15f;
		rgb2.b *= 0.886f;
		cOut += float4(rgb2, originalColor.a);
	}
	cOut /= 13.0f;
	
	float4 litColor = cOut;


	//float ScratchAmount = 0.731f;
    float ScratchAmount = 0.14f;
	float NoiseAmount = 0.0f;
	float2 RandomCoord1 = float2(0.03f, 0.46f);
	float2 RandomCoord2 = float2(0.05f, 0.13f);
	//float Frame = input.PosW.x * input.PosW.y;
    //float Frame = frac(input.PosW.x);
    //float Frame = 0.0f;
    float Frame = frac(input.Color.y);
	float ScratchAmountInv = 1.0 / ScratchAmount;

	
    //float4 color = litColor;
    float4 color = originalColor;

    // Add Scratch
    float2 sc = Frame * float2(0.001f, 0.4f);
    sc.x = frac(input.tex.x + sc.x);
    float scratch = shaderTexture.Sample(SampleType, sc).r;
    scratch = 2 * scratch * ScratchAmountInv;
    scratch = 1 - abs(1 - scratch);
    scratch = max(0, scratch);
    color.rgb += scratch.rrr; 

    // Calculate random coord + sample
    float2 rCoord = (input.tex + RandomCoord1 + RandomCoord2) * 0.33;
    float3 rand = shaderTexture.Sample(SampleType, rCoord);
    // Add noise
    if(NoiseAmount > rand.r) {
        color.rgb = 0.1 + rand.b * 0.4;
    }

    // Convert to gray + desaturated Sepia
    float gray = dot(color, float4(0.3, 0.59, 0.11, 0)); 
    color = float4(gray * float3(0.9, 0.8, 0.6) , 1); // Sepia original (0.9, 0.7, 0.3)

    // Calc distance to center
    float2 dist = 0.5 - input.tex;
    // Random light fluctuation
    float fluc = RandomCoord2.x * 0.04 - 0.02;
    // Vignette effect
    color.rgb *= (0.4 + fluc - dot(dist, dist))  * 2.8;

    //litColor = color;
	
    float4 totalC = litColor * 0.4f + color * 0.1f  + originalColor * 0.5f;
    
    float Contrast = 1.2f;
	totalC = float4(((totalC.rgb - 0.5f) * max(Contrast, 0)) + 0.5f, 1.0f);
	
	float Brightness = 0.1f;
	totalC.rgb += Brightness;
	
	//return litColor;
    //return litColor * 0.4f + color * 0.1f  + originalColor * 0.5f;
    return totalC;
}
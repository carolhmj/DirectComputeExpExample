struct OutputType {
	float exp;
	float exp2;
};

StructuredBuffer<float> Input;
RWStructuredBuffer<OutputType> Output;

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	Output[DTid.x].exp = exp(Input[DTid.x]);
	Output[DTid.x].exp2 = exp2(Input[DTid.x]);
}
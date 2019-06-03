struct OutputType {
	float directExp;
	float indirectExp;
};

StructuredBuffer<float> Input;
RWStructuredBuffer<OutputType> Output;

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	double ln2 = 1.442695040888963;
	Output[DTid.x].directExp = exp(Input[DTid.x]);
	Output[DTid.x].indirectExp = exp2(Input[DTid.x] * ln2);
}
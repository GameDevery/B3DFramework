shader Clear
{
	variations
	{
		MODE =
		{
			0, // Integer
			1, // Color
		};
	};

	depth
	{
		read = false;
		write = false;
	};

	code
	{
		struct VStoFS
		{
			float4 position : SV_Position;
		};

		struct VertexInput
		{
			float2 screenPos : POSITION;
		};

		cbuffer Params
		{
			uint4 gIntegerClearValue;
			float4 gColorClearValue;
		};

		VStoFS vsmain(VertexInput input)
		{
			VStoFS output;
			output.position = float4(input.screenPos, 0, 1);

			return output;
		}

		#if MODE == 0
		uint4 fsmain(VStoFS input) : SV_Target0
		{
			return gIntegerClearValue;
		}
		#else
		float4 fsmain(VStoFS input) : SV_Target0
		{
			return gColorClearValue;
		}
		#endif
	};
};

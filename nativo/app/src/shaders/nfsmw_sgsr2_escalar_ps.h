// Generated with tools/compilar_shader_presentador.py from app/src/shaders/nfsmw_sgsr2_escalar.frag.
#if 0
; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 10
; Bound: 1166
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %gl_FragCoord %1013
               OpExecutionMode %4 OriginUpperLeft
               OpDecorate %_arr_v4float_uint_4 ArrayStride 16
               OpMemberDecorate %_struct_37 0 Offset 0
               OpMemberDecorate %_struct_37 1 Offset 64
               OpMemberDecorate %_struct_37 2 Offset 72
               OpMemberDecorate %_struct_37 3 Offset 80
               OpMemberDecorate %_struct_37 4 Offset 88
               OpMemberDecorate %_struct_37 5 Offset 96
               OpMemberDecorate %_struct_37 6 Offset 104
               OpMemberDecorate %_struct_37 7 Offset 112
               OpMemberDecorate %_struct_37 8 Offset 120
               OpMemberDecorate %_struct_37 9 Offset 128
               OpMemberDecorate %_struct_37 10 Offset 136
               OpMemberDecorate %_struct_37 11 Offset 144
               OpMemberDecorate %_struct_37 12 Offset 148
               OpMemberDecorate %_struct_37 13 Offset 152
               OpMemberDecorate %_struct_37 14 Offset 156
               OpMemberDecorate %_struct_37 15 Offset 160
               OpMemberDecorate %_struct_37 16 Offset 164
               OpMemberDecorate %_struct_37 17 Offset 168
               OpMemberDecorate %_struct_37 18 Offset 172
               OpDecorate %_struct_37 Block
               OpDecorate %39 DescriptorSet 0
               OpDecorate %39 Binding 0
               OpDecorate %gl_FragCoord BuiltIn FragCoord
               OpDecorate %64 RelaxedPrecision
               OpDecorate %64 DescriptorSet 0
               OpDecorate %64 Binding 2
               OpDecorate %65 RelaxedPrecision
               OpDecorate %68 DescriptorSet 0
               OpDecorate %68 Binding 4
               OpDecorate %166 RelaxedPrecision
               OpDecorate %166 DescriptorSet 0
               OpDecorate %166 Binding 1
               OpDecorate %167 RelaxedPrecision
               OpDecorate %171 RelaxedPrecision
               OpDecorate %172 RelaxedPrecision
               OpDecorate %182 RelaxedPrecision
               OpDecorate %183 RelaxedPrecision
               OpDecorate %187 RelaxedPrecision
               OpDecorate %192 RelaxedPrecision
               OpDecorate %205 RelaxedPrecision
               OpDecorate %206 RelaxedPrecision
               OpDecorate %207 RelaxedPrecision
               OpDecorate %220 RelaxedPrecision
               OpDecorate %224 RelaxedPrecision
               OpDecorate %233 RelaxedPrecision
               OpDecorate %233 DescriptorSet 0
               OpDecorate %233 Binding 3
               OpDecorate %234 RelaxedPrecision
               OpDecorate %245 RelaxedPrecision
               OpDecorate %246 RelaxedPrecision
               OpDecorate %252 RelaxedPrecision
               OpDecorate %256 RelaxedPrecision
               OpDecorate %260 RelaxedPrecision
               OpDecorate %261 RelaxedPrecision
               OpDecorate %268 RelaxedPrecision
               OpDecorate %270 RelaxedPrecision
               OpDecorate %271 RelaxedPrecision
               OpDecorate %272 RelaxedPrecision
               OpDecorate %273 RelaxedPrecision
               OpDecorate %279 RelaxedPrecision
               OpDecorate %280 RelaxedPrecision
               OpDecorate %288 RelaxedPrecision
               OpDecorate %310 RelaxedPrecision
               OpDecorate %311 RelaxedPrecision
               OpDecorate %317 RelaxedPrecision
               OpDecorate %321 RelaxedPrecision
               OpDecorate %325 RelaxedPrecision
               OpDecorate %326 RelaxedPrecision
               OpDecorate %333 RelaxedPrecision
               OpDecorate %335 RelaxedPrecision
               OpDecorate %336 RelaxedPrecision
               OpDecorate %337 RelaxedPrecision
               OpDecorate %338 RelaxedPrecision
               OpDecorate %340 RelaxedPrecision
               OpDecorate %344 RelaxedPrecision
               OpDecorate %345 RelaxedPrecision
               OpDecorate %348 RelaxedPrecision
               OpDecorate %351 RelaxedPrecision
               OpDecorate %355 RelaxedPrecision
               OpDecorate %358 RelaxedPrecision
               OpDecorate %361 RelaxedPrecision
               OpDecorate %363 RelaxedPrecision
               OpDecorate %366 RelaxedPrecision
               OpDecorate %378 RelaxedPrecision
               OpDecorate %379 RelaxedPrecision
               OpDecorate %386 RelaxedPrecision
               OpDecorate %390 RelaxedPrecision
               OpDecorate %394 RelaxedPrecision
               OpDecorate %395 RelaxedPrecision
               OpDecorate %402 RelaxedPrecision
               OpDecorate %404 RelaxedPrecision
               OpDecorate %405 RelaxedPrecision
               OpDecorate %406 RelaxedPrecision
               OpDecorate %407 RelaxedPrecision
               OpDecorate %409 RelaxedPrecision
               OpDecorate %413 RelaxedPrecision
               OpDecorate %414 RelaxedPrecision
               OpDecorate %417 RelaxedPrecision
               OpDecorate %420 RelaxedPrecision
               OpDecorate %424 RelaxedPrecision
               OpDecorate %427 RelaxedPrecision
               OpDecorate %432 RelaxedPrecision
               OpDecorate %435 RelaxedPrecision
               OpDecorate %445 RelaxedPrecision
               OpDecorate %446 RelaxedPrecision
               OpDecorate %454 RelaxedPrecision
               OpDecorate %458 RelaxedPrecision
               OpDecorate %459 RelaxedPrecision
               OpDecorate %466 RelaxedPrecision
               OpDecorate %468 RelaxedPrecision
               OpDecorate %469 RelaxedPrecision
               OpDecorate %470 RelaxedPrecision
               OpDecorate %471 RelaxedPrecision
               OpDecorate %473 RelaxedPrecision
               OpDecorate %477 RelaxedPrecision
               OpDecorate %478 RelaxedPrecision
               OpDecorate %481 RelaxedPrecision
               OpDecorate %484 RelaxedPrecision
               OpDecorate %488 RelaxedPrecision
               OpDecorate %491 RelaxedPrecision
               OpDecorate %496 RelaxedPrecision
               OpDecorate %499 RelaxedPrecision
               OpDecorate %510 RelaxedPrecision
               OpDecorate %511 RelaxedPrecision
               OpDecorate %517 RelaxedPrecision
               OpDecorate %521 RelaxedPrecision
               OpDecorate %525 RelaxedPrecision
               OpDecorate %526 RelaxedPrecision
               OpDecorate %533 RelaxedPrecision
               OpDecorate %535 RelaxedPrecision
               OpDecorate %536 RelaxedPrecision
               OpDecorate %537 RelaxedPrecision
               OpDecorate %538 RelaxedPrecision
               OpDecorate %540 RelaxedPrecision
               OpDecorate %544 RelaxedPrecision
               OpDecorate %545 RelaxedPrecision
               OpDecorate %548 RelaxedPrecision
               OpDecorate %551 RelaxedPrecision
               OpDecorate %555 RelaxedPrecision
               OpDecorate %558 RelaxedPrecision
               OpDecorate %563 RelaxedPrecision
               OpDecorate %566 RelaxedPrecision
               OpDecorate %838 RelaxedPrecision
               OpDecorate %841 RelaxedPrecision
               OpDecorate %844 RelaxedPrecision
               OpDecorate %849 RelaxedPrecision
               OpDecorate %850 RelaxedPrecision
               OpDecorate %851 RelaxedPrecision
               OpDecorate %853 RelaxedPrecision
               OpDecorate %855 RelaxedPrecision
               OpDecorate %856 RelaxedPrecision
               OpDecorate %857 RelaxedPrecision
               OpDecorate %861 RelaxedPrecision
               OpDecorate %863 RelaxedPrecision
               OpDecorate %864 RelaxedPrecision
               OpDecorate %874 RelaxedPrecision
               OpDecorate %878 RelaxedPrecision
               OpDecorate %884 RelaxedPrecision
               OpDecorate %886 RelaxedPrecision
               OpDecorate %887 RelaxedPrecision
               OpDecorate %888 RelaxedPrecision
               OpDecorate %889 RelaxedPrecision
               OpDecorate %896 RelaxedPrecision
               OpDecorate %897 RelaxedPrecision
               OpDecorate %898 RelaxedPrecision
               OpDecorate %899 RelaxedPrecision
               OpDecorate %907 RelaxedPrecision
               OpDecorate %911 RelaxedPrecision
               OpDecorate %915 RelaxedPrecision
               OpDecorate %919 RelaxedPrecision
               OpDecorate %923 RelaxedPrecision
               OpDecorate %926 RelaxedPrecision
               OpDecorate %929 RelaxedPrecision
               OpDecorate %934 RelaxedPrecision
               OpDecorate %954 RelaxedPrecision
               OpDecorate %961 RelaxedPrecision
               OpDecorate %969 RelaxedPrecision
               OpDecorate %970 RelaxedPrecision
               OpDecorate %971 RelaxedPrecision
               OpDecorate %975 RelaxedPrecision
               OpDecorate %980 RelaxedPrecision
               OpDecorate %986 RelaxedPrecision
               OpDecorate %987 RelaxedPrecision
               OpDecorate %992 RelaxedPrecision
               OpDecorate %1002 RelaxedPrecision
               OpDecorate %1004 RelaxedPrecision
               OpDecorate %1005 RelaxedPrecision
               OpDecorate %1013 RelaxedPrecision
               OpDecorate %1013 Location 0
               OpDecorate %1019 RelaxedPrecision
               OpDecorate %1026 RelaxedPrecision
               OpDecorate %1029 RelaxedPrecision
               OpDecorate %1033 RelaxedPrecision
               OpDecorate %1036 RelaxedPrecision
               OpDecorate %1043 RelaxedPrecision
               OpDecorate %1046 RelaxedPrecision
               OpDecorate %1050 RelaxedPrecision
               OpDecorate %1053 RelaxedPrecision
               OpDecorate %1060 RelaxedPrecision
               OpDecorate %1063 RelaxedPrecision
               OpDecorate %1067 RelaxedPrecision
               OpDecorate %1070 RelaxedPrecision
               OpDecorate %1077 RelaxedPrecision
               OpDecorate %1080 RelaxedPrecision
               OpDecorate %1084 RelaxedPrecision
               OpDecorate %1087 RelaxedPrecision
               OpDecorate %1094 RelaxedPrecision
               OpDecorate %1097 RelaxedPrecision
               OpDecorate %1101 RelaxedPrecision
               OpDecorate %1104 RelaxedPrecision
               OpDecorate %1165 RelaxedPrecision
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %float_1 = OpConstant %float 1
 %float_0_75 = OpConstant %float 0.75
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
     %uint_4 = OpConstant %uint 4
%_arr_v4float_uint_4 = OpTypeArray %v4float %uint_4
    %v2float = OpTypeVector %float 2
 %_struct_37 = OpTypeStruct %_arr_v4float_uint_4 %v2float %v2float %v2float %v2float %v2float %v2float %v2float %v2float %v2float %v2float %float %float %float %float %float %float %float %float
%_ptr_Uniform__struct_37 = OpTypePointer Uniform %_struct_37
         %39 = OpVariable %_ptr_Uniform__struct_37 Uniform
        %int = OpTypeInt 32 1
      %int_6 = OpConstant %int 6
     %uint_0 = OpConstant %uint 0
%_ptr_Uniform_float = OpTypePointer Uniform %float
     %uint_1 = OpConstant %uint 1
%_ptr_Input_v4float = OpTypePointer Input %v4float
%gl_FragCoord = OpVariable %_ptr_Input_v4float Input
      %int_4 = OpConstant %int 4
%_ptr_Uniform_v2float = OpTypePointer Uniform %v2float
         %62 = OpTypeImage %float 2D 0 0 0 1 Unknown
%_ptr_UniformConstant_62 = OpTypePointer UniformConstant %62
         %64 = OpVariable %_ptr_UniformConstant_62 UniformConstant
         %66 = OpTypeSampler
%_ptr_UniformConstant_66 = OpTypePointer UniformConstant %66
         %68 = OpVariable %_ptr_UniformConstant_66 UniformConstant
         %70 = OpTypeSampledImage %62
    %float_0 = OpConstant %float 0
%float_0_00999999978 = OpConstant %float 0.00999999978
       %bool = OpTypeBool
  %float_0_5 = OpConstant %float 0.5
         %88 = OpConstantComposite %v2float %float_0 %float_0
      %int_5 = OpConstant %int 5
      %v2int = OpTypeVector %int 2
      %int_1 = OpConstant %int 1
        %101 = OpConstantComposite %v2int %int_1 %int_1
      %int_3 = OpConstant %int 3
    %v3float = OpTypeVector %float 3
 %float_n0_5 = OpConstant %float -0.5
        %166 = OpVariable %_ptr_UniformConstant_62 UniformConstant
%float_0_300000012 = OpConstant %float 0.300000012
 %float_0_25 = OpConstant %float 0.25
      %int_2 = OpConstant %int 2
   %float_n2 = OpConstant %float -2
   %float_n3 = OpConstant %float -3
%float_0_0199999996 = OpConstant %float 0.0199999996
        %215 = OpConstantComposite %v2float %float_0_5 %float_0_5
        %233 = OpVariable %_ptr_UniformConstant_62 UniformConstant
      %int_0 = OpConstant %int 0
        %239 = OpConstantComposite %v2int %int_0 %int_1
        %241 = OpConstantComposite %v2int %int_0 %int_0
        %251 = OpConstantComposite %v2float %float_0 %float_1
        %305 = OpConstantComposite %v2int %int_1 %int_0
        %316 = OpConstantComposite %v2float %float_1 %float_0
     %int_n1 = OpConstant %int -1
        %373 = OpConstantComposite %v2int %int_n1 %int_0
   %float_n1 = OpConstant %float -1
        %385 = OpConstantComposite %v2float %float_n1 %float_0
        %505 = OpConstantComposite %v2int %int_0 %int_n1
        %516 = OpConstantComposite %v2float %float_0 %float_n1
%float_0_075000003 = OpConstant %float 0.075000003
        %860 = OpConstantComposite %v3float %float_0_075000003 %float_0_075000003 %float_0_075000003
%float_0_333333343 = OpConstant %float 0.333333343
   %float_10 = OpConstant %float 10
%float_0_0500000007 = OpConstant %float 0.0500000007
     %int_12 = OpConstant %int 12
%float_9_99999997en07 = OpConstant %float 9.99999997e-07
     %v3bool = OpTypeVector %bool 3
%float_0_100000001 = OpConstant %float 0.100000001
%float_1_19200003en07 = OpConstant %float 1.19200003e-07
     %int_13 = OpConstant %int 13
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %1013 = OpVariable %_ptr_Output_v4float Output
       %1159 = OpConstantNull %v3float
%float_3_33333349 = OpConstant %float 3.33333349
          %4 = OpFunction %void None %3
          %5 = OpLabel
         %44 = OpAccessChain %_ptr_Uniform_float %39 %int_6 %uint_0
         %45 = OpLoad %float %44
         %48 = OpAccessChain %_ptr_Uniform_float %39 %int_6 %uint_1
         %49 = OpLoad %float %48
         %54 = OpLoad %v4float %gl_FragCoord
         %55 = OpVectorShuffle %v2float %54 %54 0 1
         %58 = OpAccessChain %_ptr_Uniform_v2float %39 %int_4
         %59 = OpLoad %v2float %58
         %60 = OpFMul %v2float %55 %59
         %65 = OpLoad %62 %64
         %69 = OpLoad %66 %68
         %71 = OpSampledImage %70 %65 %69
         %74 = OpImageSampleExplicitLod %v4float %71 %60 Lod %float_0
         %76 = OpCompositeExtract %float %74 3
         %79 = OpFOrdGreaterThan %bool %76 %float_0_00999999978
         %80 = OpSelect %float %79 %float_1 %float_0
         %84 = OpFOrdGreaterThan %bool %80 %float_0_5
               OpSelectionMerge %87 None
               OpBranchConditional %84 %86 %89
         %89 = OpLabel
         %91 = OpAccessChain %_ptr_Uniform_v2float %39 %int_5
         %92 = OpLoad %v2float %91
               OpBranch %87
         %86 = OpLabel
               OpBranch %87
         %87 = OpLabel
       %1144 = OpPhi %v2float %92 %89 %88 %86
         %98 = OpAccessChain %_ptr_Uniform_v2float %39 %int_1
         %99 = OpLoad %v2float %98
        %100 = OpConvertFToS %v2int %99
        %102 = OpISub %v2int %100 %101
        %105 = OpCompositeExtract %float %60 0
        %107 = OpCompositeExtract %float %1144 0
        %109 = OpAccessChain %_ptr_Uniform_float %39 %int_3 %uint_0
        %110 = OpLoad %float %109
        %112 = OpExtInst %float %1 Fma %107 %110 %105
        %113 = OpExtInst %float %1 FClamp %112 %float_0 %float_1
        %116 = OpCompositeExtract %float %60 1
        %118 = OpCompositeExtract %float %1144 1
        %119 = OpAccessChain %_ptr_Uniform_float %39 %int_3 %uint_1
        %120 = OpLoad %float %119
        %122 = OpExtInst %float %1 Fma %118 %120 %116
        %123 = OpExtInst %float %1 FClamp %122 %float_0 %float_1
       %1158 = OpCompositeConstruct %v2float %113 %123
        %129 = OpFMul %v2float %1158 %99
        %130 = OpConvertFToS %v2int %129
        %136 = OpSampledImage %70 %65 %69
        %138 = OpImageSampleExplicitLod %v4float %136 %1158 Lod %float_0
        %142 = OpVectorShuffle %v2float %138 %1159 0 1
        %146 = OpCompositeExtract %float %138 0
        %150 = OpExtInst %float %1 Fma %float_n0_5 %146 %105
        %151 = OpExtInst %float %1 FClamp %150 %float_0 %float_1
        %154 = OpCompositeExtract %float %138 1
        %158 = OpExtInst %float %1 Fma %float_0_5 %154 %116
        %159 = OpExtInst %float %1 FClamp %158 %float_0 %float_1
       %1160 = OpCompositeConstruct %v2float %151 %159
        %164 = OpCompositeExtract %float %138 2
        %167 = OpLoad %62 %166
        %169 = OpSampledImage %70 %167 %69
        %171 = OpImageSampleExplicitLod %v4float %169 %1160 Lod %float_0
        %172 = OpVectorShuffle %v3float %171 %171 0 1 2
        %182 = OpExtInst %float %1 Fma %float_0_300000012 %45 %float_0_300000012
        %183 = OpExtInst %float %1 FMax %float_1 %182
        %187 = OpFMul %float %float_0_25 %164
        %192 = OpExtInst %float %1 FMix %45 %183 %187
        %196 = OpAccessChain %_ptr_Uniform_v2float %39 %int_2
        %197 = OpLoad %v2float %196
        %198 = OpFMul %v2float %142 %197
        %199 = OpExtInst %float %1 Length %198
        %205 = OpFMul %float %199 %float_0_0199999996
        %206 = OpExtInst %float %1 FClamp %205 %float_0 %float_1
        %207 = OpExtInst %float %1 FMix %float_n2 %float_n3 %206
        %214 = OpConvertSToF %v2float %130
        %216 = OpFAdd %v2float %214 %215
        %218 = OpFSub %v2float %216 %1144
        %220 = OpFMul %float %192 %float_0_5
        %224 = OpFMul %float %220 %220
       %1161 = OpFNegate %v2float %60
        %231 = OpExtInst %v2float %1 Fma %1161 %99 %218
        %234 = OpLoad %62 %233
        %236 = OpSampledImage %70 %234 %69
        %240 = OpIAdd %v2int %130 %239
        %243 = OpExtInst %v2int %1 SClamp %240 %241 %102
        %244 = OpImage %62 %236
        %245 = OpImageFetch %v4float %244 %243 Lod %int_0
        %246 = OpVectorShuffle %v3float %245 %245 0 1 2
        %252 = OpFAdd %v2float %231 %251
        %256 = OpDot %float %252 %252
        %260 = OpFMul %float %256 %224
        %261 = OpExtInst %float %1 FClamp %260 %float_0 %float_1
       %1026 = OpFSub %float %261 %float_1
       %1029 = OpFMul %float %1026 %1026
       %1033 = OpExtInst %float %1 Fma %float_0_75 %1026 %1029
       %1036 = OpFMul %float %1033 %1029
        %268 = OpVectorTimesScalar %v3float %246 %1036
        %270 = OpCompositeExtract %float %268 0
        %271 = OpCompositeExtract %float %268 1
        %272 = OpCompositeExtract %float %268 2
        %273 = OpCompositeConstruct %v4float %270 %271 %272 %1036
        %279 = OpFMul %float %256 %207
        %280 = OpExtInst %float %1 Exp %279
        %288 = OpVectorTimesScalar %v3float %246 %280
        %303 = OpSampledImage %70 %234 %69
        %306 = OpIAdd %v2int %130 %305
        %308 = OpExtInst %v2int %1 SClamp %306 %241 %102
        %309 = OpImage %62 %303
        %310 = OpImageFetch %v4float %309 %308 Lod %int_0
        %311 = OpVectorShuffle %v3float %310 %310 0 1 2
        %317 = OpFAdd %v2float %231 %316
        %321 = OpDot %float %317 %317
        %325 = OpFMul %float %321 %224
        %326 = OpExtInst %float %1 FClamp %325 %float_0 %float_1
       %1043 = OpFSub %float %326 %float_1
       %1046 = OpFMul %float %1043 %1043
       %1050 = OpExtInst %float %1 Fma %float_0_75 %1043 %1046
       %1053 = OpFMul %float %1050 %1046
        %333 = OpVectorTimesScalar %v3float %311 %1053
        %335 = OpCompositeExtract %float %333 0
        %336 = OpCompositeExtract %float %333 1
        %337 = OpCompositeExtract %float %333 2
        %338 = OpCompositeConstruct %v4float %335 %336 %337 %1053
        %340 = OpFAdd %v4float %273 %338
        %344 = OpFMul %float %321 %207
        %345 = OpExtInst %float %1 Exp %344
        %348 = OpExtInst %v3float %1 FMin %246 %311
        %351 = OpExtInst %v3float %1 FMax %246 %311
        %355 = OpVectorTimesScalar %v3float %311 %345
        %358 = OpFAdd %v3float %288 %355
        %361 = OpFMul %v3float %311 %355
        %363 = OpExtInst %v3float %1 Fma %246 %288 %361
        %366 = OpFAdd %float %280 %345
        %370 = OpSampledImage %70 %234 %69
        %374 = OpIAdd %v2int %130 %373
        %376 = OpExtInst %v2int %1 SClamp %374 %241 %102
        %377 = OpImage %62 %370
        %378 = OpImageFetch %v4float %377 %376 Lod %int_0
        %379 = OpVectorShuffle %v3float %378 %378 0 1 2
        %386 = OpFAdd %v2float %231 %385
        %390 = OpDot %float %386 %386
        %394 = OpFMul %float %390 %224
        %395 = OpExtInst %float %1 FClamp %394 %float_0 %float_1
       %1060 = OpFSub %float %395 %float_1
       %1063 = OpFMul %float %1060 %1060
       %1067 = OpExtInst %float %1 Fma %float_0_75 %1060 %1063
       %1070 = OpFMul %float %1067 %1063
        %402 = OpVectorTimesScalar %v3float %379 %1070
        %404 = OpCompositeExtract %float %402 0
        %405 = OpCompositeExtract %float %402 1
        %406 = OpCompositeExtract %float %402 2
        %407 = OpCompositeConstruct %v4float %404 %405 %406 %1070
        %409 = OpFAdd %v4float %340 %407
        %413 = OpFMul %float %390 %207
        %414 = OpExtInst %float %1 Exp %413
        %417 = OpExtInst %v3float %1 FMin %348 %379
        %420 = OpExtInst %v3float %1 FMax %351 %379
        %424 = OpVectorTimesScalar %v3float %379 %414
        %427 = OpFAdd %v3float %358 %424
        %432 = OpExtInst %v3float %1 Fma %379 %424 %363
        %435 = OpFAdd %float %366 %414
        %439 = OpSampledImage %70 %234 %69
        %443 = OpExtInst %v2int %1 SClamp %130 %241 %102
        %444 = OpImage %62 %439
        %445 = OpImageFetch %v4float %444 %443 Lod %int_0
        %446 = OpVectorShuffle %v3float %445 %445 0 1 2
        %454 = OpDot %float %231 %231
        %458 = OpFMul %float %454 %224
        %459 = OpExtInst %float %1 FClamp %458 %float_0 %float_1
       %1077 = OpFSub %float %459 %float_1
       %1080 = OpFMul %float %1077 %1077
       %1084 = OpExtInst %float %1 Fma %float_0_75 %1077 %1080
       %1087 = OpFMul %float %1084 %1080
        %466 = OpVectorTimesScalar %v3float %446 %1087
        %468 = OpCompositeExtract %float %466 0
        %469 = OpCompositeExtract %float %466 1
        %470 = OpCompositeExtract %float %466 2
        %471 = OpCompositeConstruct %v4float %468 %469 %470 %1087
        %473 = OpFAdd %v4float %409 %471
        %477 = OpFMul %float %454 %207
        %478 = OpExtInst %float %1 Exp %477
        %481 = OpExtInst %v3float %1 FMin %417 %446
        %484 = OpExtInst %v3float %1 FMax %420 %446
        %488 = OpVectorTimesScalar %v3float %446 %478
        %491 = OpFAdd %v3float %427 %488
        %496 = OpExtInst %v3float %1 Fma %446 %488 %432
        %499 = OpFAdd %float %435 %478
        %503 = OpSampledImage %70 %234 %69
        %506 = OpIAdd %v2int %130 %505
        %508 = OpExtInst %v2int %1 SClamp %506 %241 %102
        %509 = OpImage %62 %503
        %510 = OpImageFetch %v4float %509 %508 Lod %int_0
        %511 = OpVectorShuffle %v3float %510 %510 0 1 2
        %517 = OpFAdd %v2float %231 %516
        %521 = OpDot %float %517 %517
        %525 = OpFMul %float %521 %224
        %526 = OpExtInst %float %1 FClamp %525 %float_0 %float_1
       %1094 = OpFSub %float %526 %float_1
       %1097 = OpFMul %float %1094 %1094
       %1101 = OpExtInst %float %1 Fma %float_0_75 %1094 %1097
       %1104 = OpFMul %float %1101 %1097
        %533 = OpVectorTimesScalar %v3float %511 %1104
        %535 = OpCompositeExtract %float %533 0
        %536 = OpCompositeExtract %float %533 1
        %537 = OpCompositeExtract %float %533 2
        %538 = OpCompositeConstruct %v4float %535 %536 %537 %1104
        %540 = OpFAdd %v4float %473 %538
        %544 = OpFMul %float %521 %207
        %545 = OpExtInst %float %1 Exp %544
        %548 = OpExtInst %v3float %1 FMin %481 %511
        %551 = OpExtInst %v3float %1 FMax %484 %511
        %555 = OpVectorTimesScalar %v3float %511 %545
        %558 = OpFAdd %v3float %491 %555
        %563 = OpExtInst %v3float %1 Fma %511 %555 %496
        %566 = OpFAdd %float %499 %545
        %838 = OpFDiv %float %float_1 %566
        %841 = OpVectorTimesScalar %v3float %558 %838
        %844 = OpVectorTimesScalar %v3float %563 %838
       %1162 = OpFNegate %v3float %841
        %849 = OpExtInst %v3float %1 Fma %1162 %841 %844
        %850 = OpExtInst %v3float %1 FAbs %849
        %851 = OpExtInst %v3float %1 Sqrt %850
        %853 = OpVectorShuffle %v3float %540 %540 0 1 2
        %855 = OpCompositeExtract %float %540 3
        %856 = OpCompositeConstruct %v3float %855 %855 %855
        %857 = OpFDiv %v3float %853 %856
        %861 = OpFSub %v3float %548 %860
        %863 = OpFAdd %v3float %551 %860
        %864 = OpExtInst %v3float %1 FClamp %857 %861 %863
        %866 = OpCompositeExtract %float %864 0
        %868 = OpCompositeExtract %float %864 1
        %870 = OpCompositeExtract %float %864 2
        %874 = OpFMul %float %855 %float_0_333333343
       %1163 = OpCompositeConstruct %v4float %866 %868 %870 %874
        %878 = OpFSub %float %float_1 %164
        %884 = OpFMul %float %855 %float_3_33333349
        %886 = OpFMul %float %float_10 %199
        %887 = OpExtInst %float %1 FClamp %886 %float_0 %float_1
        %888 = OpExtInst %float %1 FMix %878 %884 %887
        %889 = OpExtInst %float %1 FMin %878 %888
        %896 = OpFMul %float %199 %float_0_0500000007
        %897 = OpExtInst %float %1 FClamp %896 %float_0 %float_1
        %898 = OpExtInst %float %1 FMix %889 %874 %897
        %899 = OpExtInst %float %1 FMin %889 %898
        %907 = OpExtInst %float %1 FMax %164 %897
        %911 = OpExtInst %float %1 FMix %49 %float_1 %907
        %915 = OpVectorTimesScalar %v3float %851 %911
        %919 = OpFSub %v3float %841 %915
        %923 = OpFAdd %v3float %841 %915
        %926 = OpExtInst %v3float %1 FMin %551 %923
        %929 = OpExtInst %v3float %1 FMax %548 %919
        %934 = OpExtInst %v3float %1 FClamp %172 %929 %926
        %937 = OpAccessChain %_ptr_Uniform_float %39 %int_12
        %938 = OpLoad %float %937
        %941 = OpExtInst %float %1 FAbs %146
        %944 = OpExtInst %float %1 FAbs %154
        %945 = OpFAdd %float %941 %944
        %947 = OpFOrdGreaterThan %bool %945 %float_9_99999997en07
       %1165 = OpSelect %float %947 %float_0 %938
        %954 = OpFOrdGreaterThan %v3bool %929 %172
        %955 = OpAny %bool %954
        %956 = OpLogicalNot %bool %955
               OpSelectionMerge %958 None
               OpBranchConditional %956 %957 %958
        %957 = OpLabel
        %961 = OpFOrdGreaterThan %v3bool %172 %926
        %962 = OpAny %bool %961
               OpBranch %958
        %958 = OpLabel
        %963 = OpPhi %bool %955 %87 %962 %957
        %965 = OpSelect %float %963 %1165 %float_1
        %969 = OpExtInst %float %1 FClamp %965 %float_0 %float_1
        %970 = OpCompositeConstruct %v3float %969 %969 %969
        %971 = OpExtInst %v3float %1 FMix %934 %172 %970
        %975 = OpExtInst %float %1 FMin %899 %float_0_100000001
        %980 = OpExtInst %float %1 FMix %975 %899 %969
        %986 = OpExtInst %float %1 Fma %855 %float_0_333333343 %980
        %987 = OpExtInst %float %1 FMax %float_1_19200003en07 %986
        %992 = OpFDiv %float %874 %987
        %994 = OpAccessChain %_ptr_Uniform_float %39 %int_13
        %995 = OpLoad %float %994
        %996 = OpFAdd %float %992 %995
        %998 = OpFAdd %float %996 %80
        %999 = OpExtInst %float %1 FClamp %998 %float_0 %float_1
       %1002 = OpVectorShuffle %v3float %1163 %1163 0 1 2
       %1004 = OpCompositeConstruct %v3float %999 %999 %999
       %1005 = OpExtInst %v3float %1 FMix %971 %1002 %1004
       %1007 = OpCompositeExtract %float %1005 0
       %1009 = OpCompositeExtract %float %1005 1
       %1011 = OpCompositeExtract %float %1005 2
       %1019 = OpCompositeConstruct %v4float %1007 %1009 %1011 %float_1
               OpStore %1013 %1019
               OpReturn
               OpFunctionEnd
#endif

const uint32_t nfsmw_sgsr2_escalar_ps[] = {
    0x07230203, 0x00010000, 0x000D000A, 0x0000048E, 0x00000000, 0x00020011, 0x00000001, 0x0006000B,
    0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 0x00000000, 0x0003000E, 0x00000000, 0x00000001,
    0x0007000F, 0x00000004, 0x00000004, 0x6E69616D, 0x00000000, 0x00000035, 0x000003F5, 0x00030010,
    0x00000004, 0x00000007, 0x00040047, 0x00000023, 0x00000006, 0x00000010, 0x00050048, 0x00000025,
    0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x00000025, 0x00000001, 0x00000023, 0x00000040,
    0x00050048, 0x00000025, 0x00000002, 0x00000023, 0x00000048, 0x00050048, 0x00000025, 0x00000003,
    0x00000023, 0x00000050, 0x00050048, 0x00000025, 0x00000004, 0x00000023, 0x00000058, 0x00050048,
    0x00000025, 0x00000005, 0x00000023, 0x00000060, 0x00050048, 0x00000025, 0x00000006, 0x00000023,
    0x00000068, 0x00050048, 0x00000025, 0x00000007, 0x00000023, 0x00000070, 0x00050048, 0x00000025,
    0x00000008, 0x00000023, 0x00000078, 0x00050048, 0x00000025, 0x00000009, 0x00000023, 0x00000080,
    0x00050048, 0x00000025, 0x0000000A, 0x00000023, 0x00000088, 0x00050048, 0x00000025, 0x0000000B,
    0x00000023, 0x00000090, 0x00050048, 0x00000025, 0x0000000C, 0x00000023, 0x00000094, 0x00050048,
    0x00000025, 0x0000000D, 0x00000023, 0x00000098, 0x00050048, 0x00000025, 0x0000000E, 0x00000023,
    0x0000009C, 0x00050048, 0x00000025, 0x0000000F, 0x00000023, 0x000000A0, 0x00050048, 0x00000025,
    0x00000010, 0x00000023, 0x000000A4, 0x00050048, 0x00000025, 0x00000011, 0x00000023, 0x000000A8,
    0x00050048, 0x00000025, 0x00000012, 0x00000023, 0x000000AC, 0x00030047, 0x00000025, 0x00000002,
    0x00040047, 0x00000027, 0x00000022, 0x00000000, 0x00040047, 0x00000027, 0x00000021, 0x00000000,
    0x00040047, 0x00000035, 0x0000000B, 0x0000000F, 0x00030047, 0x00000040, 0x00000000, 0x00040047,
    0x00000040, 0x00000022, 0x00000000, 0x00040047, 0x00000040, 0x00000021, 0x00000002, 0x00030047,
    0x00000041, 0x00000000, 0x00040047, 0x00000044, 0x00000022, 0x00000000, 0x00040047, 0x00000044,
    0x00000021, 0x00000004, 0x00030047, 0x000000A6, 0x00000000, 0x00040047, 0x000000A6, 0x00000022,
    0x00000000, 0x00040047, 0x000000A6, 0x00000021, 0x00000001, 0x00030047, 0x000000A7, 0x00000000,
    0x00030047, 0x000000AB, 0x00000000, 0x00030047, 0x000000AC, 0x00000000, 0x00030047, 0x000000B6,
    0x00000000, 0x00030047, 0x000000B7, 0x00000000, 0x00030047, 0x000000BB, 0x00000000, 0x00030047,
    0x000000C0, 0x00000000, 0x00030047, 0x000000CD, 0x00000000, 0x00030047, 0x000000CE, 0x00000000,
    0x00030047, 0x000000CF, 0x00000000, 0x00030047, 0x000000DC, 0x00000000, 0x00030047, 0x000000E0,
    0x00000000, 0x00030047, 0x000000E9, 0x00000000, 0x00040047, 0x000000E9, 0x00000022, 0x00000000,
    0x00040047, 0x000000E9, 0x00000021, 0x00000003, 0x00030047, 0x000000EA, 0x00000000, 0x00030047,
    0x000000F5, 0x00000000, 0x00030047, 0x000000F6, 0x00000000, 0x00030047, 0x000000FC, 0x00000000,
    0x00030047, 0x00000100, 0x00000000, 0x00030047, 0x00000104, 0x00000000, 0x00030047, 0x00000105,
    0x00000000, 0x00030047, 0x0000010C, 0x00000000, 0x00030047, 0x0000010E, 0x00000000, 0x00030047,
    0x0000010F, 0x00000000, 0x00030047, 0x00000110, 0x00000000, 0x00030047, 0x00000111, 0x00000000,
    0x00030047, 0x00000117, 0x00000000, 0x00030047, 0x00000118, 0x00000000, 0x00030047, 0x00000120,
    0x00000000, 0x00030047, 0x00000136, 0x00000000, 0x00030047, 0x00000137, 0x00000000, 0x00030047,
    0x0000013D, 0x00000000, 0x00030047, 0x00000141, 0x00000000, 0x00030047, 0x00000145, 0x00000000,
    0x00030047, 0x00000146, 0x00000000, 0x00030047, 0x0000014D, 0x00000000, 0x00030047, 0x0000014F,
    0x00000000, 0x00030047, 0x00000150, 0x00000000, 0x00030047, 0x00000151, 0x00000000, 0x00030047,
    0x00000152, 0x00000000, 0x00030047, 0x00000154, 0x00000000, 0x00030047, 0x00000158, 0x00000000,
    0x00030047, 0x00000159, 0x00000000, 0x00030047, 0x0000015C, 0x00000000, 0x00030047, 0x0000015F,
    0x00000000, 0x00030047, 0x00000163, 0x00000000, 0x00030047, 0x00000166, 0x00000000, 0x00030047,
    0x00000169, 0x00000000, 0x00030047, 0x0000016B, 0x00000000, 0x00030047, 0x0000016E, 0x00000000,
    0x00030047, 0x0000017A, 0x00000000, 0x00030047, 0x0000017B, 0x00000000, 0x00030047, 0x00000182,
    0x00000000, 0x00030047, 0x00000186, 0x00000000, 0x00030047, 0x0000018A, 0x00000000, 0x00030047,
    0x0000018B, 0x00000000, 0x00030047, 0x00000192, 0x00000000, 0x00030047, 0x00000194, 0x00000000,
    0x00030047, 0x00000195, 0x00000000, 0x00030047, 0x00000196, 0x00000000, 0x00030047, 0x00000197,
    0x00000000, 0x00030047, 0x00000199, 0x00000000, 0x00030047, 0x0000019D, 0x00000000, 0x00030047,
    0x0000019E, 0x00000000, 0x00030047, 0x000001A1, 0x00000000, 0x00030047, 0x000001A4, 0x00000000,
    0x00030047, 0x000001A8, 0x00000000, 0x00030047, 0x000001AB, 0x00000000, 0x00030047, 0x000001B0,
    0x00000000, 0x00030047, 0x000001B3, 0x00000000, 0x00030047, 0x000001BD, 0x00000000, 0x00030047,
    0x000001BE, 0x00000000, 0x00030047, 0x000001C6, 0x00000000, 0x00030047, 0x000001CA, 0x00000000,
    0x00030047, 0x000001CB, 0x00000000, 0x00030047, 0x000001D2, 0x00000000, 0x00030047, 0x000001D4,
    0x00000000, 0x00030047, 0x000001D5, 0x00000000, 0x00030047, 0x000001D6, 0x00000000, 0x00030047,
    0x000001D7, 0x00000000, 0x00030047, 0x000001D9, 0x00000000, 0x00030047, 0x000001DD, 0x00000000,
    0x00030047, 0x000001DE, 0x00000000, 0x00030047, 0x000001E1, 0x00000000, 0x00030047, 0x000001E4,
    0x00000000, 0x00030047, 0x000001E8, 0x00000000, 0x00030047, 0x000001EB, 0x00000000, 0x00030047,
    0x000001F0, 0x00000000, 0x00030047, 0x000001F3, 0x00000000, 0x00030047, 0x000001FE, 0x00000000,
    0x00030047, 0x000001FF, 0x00000000, 0x00030047, 0x00000205, 0x00000000, 0x00030047, 0x00000209,
    0x00000000, 0x00030047, 0x0000020D, 0x00000000, 0x00030047, 0x0000020E, 0x00000000, 0x00030047,
    0x00000215, 0x00000000, 0x00030047, 0x00000217, 0x00000000, 0x00030047, 0x00000218, 0x00000000,
    0x00030047, 0x00000219, 0x00000000, 0x00030047, 0x0000021A, 0x00000000, 0x00030047, 0x0000021C,
    0x00000000, 0x00030047, 0x00000220, 0x00000000, 0x00030047, 0x00000221, 0x00000000, 0x00030047,
    0x00000224, 0x00000000, 0x00030047, 0x00000227, 0x00000000, 0x00030047, 0x0000022B, 0x00000000,
    0x00030047, 0x0000022E, 0x00000000, 0x00030047, 0x00000233, 0x00000000, 0x00030047, 0x00000236,
    0x00000000, 0x00030047, 0x00000346, 0x00000000, 0x00030047, 0x00000349, 0x00000000, 0x00030047,
    0x0000034C, 0x00000000, 0x00030047, 0x00000351, 0x00000000, 0x00030047, 0x00000352, 0x00000000,
    0x00030047, 0x00000353, 0x00000000, 0x00030047, 0x00000355, 0x00000000, 0x00030047, 0x00000357,
    0x00000000, 0x00030047, 0x00000358, 0x00000000, 0x00030047, 0x00000359, 0x00000000, 0x00030047,
    0x0000035D, 0x00000000, 0x00030047, 0x0000035F, 0x00000000, 0x00030047, 0x00000360, 0x00000000,
    0x00030047, 0x0000036A, 0x00000000, 0x00030047, 0x0000036E, 0x00000000, 0x00030047, 0x00000374,
    0x00000000, 0x00030047, 0x00000376, 0x00000000, 0x00030047, 0x00000377, 0x00000000, 0x00030047,
    0x00000378, 0x00000000, 0x00030047, 0x00000379, 0x00000000, 0x00030047, 0x00000380, 0x00000000,
    0x00030047, 0x00000381, 0x00000000, 0x00030047, 0x00000382, 0x00000000, 0x00030047, 0x00000383,
    0x00000000, 0x00030047, 0x0000038B, 0x00000000, 0x00030047, 0x0000038F, 0x00000000, 0x00030047,
    0x00000393, 0x00000000, 0x00030047, 0x00000397, 0x00000000, 0x00030047, 0x0000039B, 0x00000000,
    0x00030047, 0x0000039E, 0x00000000, 0x00030047, 0x000003A1, 0x00000000, 0x00030047, 0x000003A6,
    0x00000000, 0x00030047, 0x000003BA, 0x00000000, 0x00030047, 0x000003C1, 0x00000000, 0x00030047,
    0x000003C9, 0x00000000, 0x00030047, 0x000003CA, 0x00000000, 0x00030047, 0x000003CB, 0x00000000,
    0x00030047, 0x000003CF, 0x00000000, 0x00030047, 0x000003D4, 0x00000000, 0x00030047, 0x000003DA,
    0x00000000, 0x00030047, 0x000003DB, 0x00000000, 0x00030047, 0x000003E0, 0x00000000, 0x00030047,
    0x000003EA, 0x00000000, 0x00030047, 0x000003EC, 0x00000000, 0x00030047, 0x000003ED, 0x00000000,
    0x00030047, 0x000003F5, 0x00000000, 0x00040047, 0x000003F5, 0x0000001E, 0x00000000, 0x00030047,
    0x000003FB, 0x00000000, 0x00030047, 0x00000402, 0x00000000, 0x00030047, 0x00000405, 0x00000000,
    0x00030047, 0x00000409, 0x00000000, 0x00030047, 0x0000040C, 0x00000000, 0x00030047, 0x00000413,
    0x00000000, 0x00030047, 0x00000416, 0x00000000, 0x00030047, 0x0000041A, 0x00000000, 0x00030047,
    0x0000041D, 0x00000000, 0x00030047, 0x00000424, 0x00000000, 0x00030047, 0x00000427, 0x00000000,
    0x00030047, 0x0000042B, 0x00000000, 0x00030047, 0x0000042E, 0x00000000, 0x00030047, 0x00000435,
    0x00000000, 0x00030047, 0x00000438, 0x00000000, 0x00030047, 0x0000043C, 0x00000000, 0x00030047,
    0x0000043F, 0x00000000, 0x00030047, 0x00000446, 0x00000000, 0x00030047, 0x00000449, 0x00000000,
    0x00030047, 0x0000044D, 0x00000000, 0x00030047, 0x00000450, 0x00000000, 0x00030047, 0x0000048D,
    0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006,
    0x00000020, 0x0004002B, 0x00000006, 0x0000000E, 0x3F800000, 0x0004002B, 0x00000006, 0x00000015,
    0x3F400000, 0x00040017, 0x00000020, 0x00000006, 0x00000004, 0x00040015, 0x00000021, 0x00000020,
    0x00000000, 0x0004002B, 0x00000021, 0x00000022, 0x00000004, 0x0004001C, 0x00000023, 0x00000020,
    0x00000022, 0x00040017, 0x00000024, 0x00000006, 0x00000002, 0x0015001E, 0x00000025, 0x00000023,
    0x00000024, 0x00000024, 0x00000024, 0x00000024, 0x00000024, 0x00000024, 0x00000024, 0x00000024,
    0x00000024, 0x00000024, 0x00000006, 0x00000006, 0x00000006, 0x00000006, 0x00000006, 0x00000006,
    0x00000006, 0x00000006, 0x00040020, 0x00000026, 0x00000002, 0x00000025, 0x0004003B, 0x00000026,
    0x00000027, 0x00000002, 0x00040015, 0x00000028, 0x00000020, 0x00000001, 0x0004002B, 0x00000028,
    0x00000029, 0x00000006, 0x0004002B, 0x00000021, 0x0000002A, 0x00000000, 0x00040020, 0x0000002B,
    0x00000002, 0x00000006, 0x0004002B, 0x00000021, 0x0000002F, 0x00000001, 0x00040020, 0x00000034,
    0x00000001, 0x00000020, 0x0004003B, 0x00000034, 0x00000035, 0x00000001, 0x0004002B, 0x00000028,
    0x00000038, 0x00000004, 0x00040020, 0x00000039, 0x00000002, 0x00000024, 0x00090019, 0x0000003E,
    0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x00040020,
    0x0000003F, 0x00000000, 0x0000003E, 0x0004003B, 0x0000003F, 0x00000040, 0x00000000, 0x0002001A,
    0x00000042, 0x00040020, 0x00000043, 0x00000000, 0x00000042, 0x0004003B, 0x00000043, 0x00000044,
    0x00000000, 0x0003001B, 0x00000046, 0x0000003E, 0x0004002B, 0x00000006, 0x00000049, 0x00000000,
    0x0004002B, 0x00000006, 0x0000004D, 0x3C23D70A, 0x00020014, 0x0000004E, 0x0004002B, 0x00000006,
    0x00000053, 0x3F000000, 0x0005002C, 0x00000024, 0x00000058, 0x00000049, 0x00000049, 0x0004002B,
    0x00000028, 0x0000005A, 0x00000005, 0x00040017, 0x0000005E, 0x00000028, 0x00000002, 0x0004002B,
    0x00000028, 0x00000061, 0x00000001, 0x0005002C, 0x0000005E, 0x00000065, 0x00000061, 0x00000061,
    0x0004002B, 0x00000028, 0x0000006C, 0x00000003, 0x00040017, 0x00000083, 0x00000006, 0x00000003,
    0x0004002B, 0x00000006, 0x00000090, 0xBF000000, 0x0004003B, 0x0000003F, 0x000000A6, 0x00000000,
    0x0004002B, 0x00000006, 0x000000B3, 0x3E99999A, 0x0004002B, 0x00000006, 0x000000B9, 0x3E800000,
    0x0004002B, 0x00000028, 0x000000C3, 0x00000002, 0x0004002B, 0x00000006, 0x000000C9, 0xC0000000,
    0x0004002B, 0x00000006, 0x000000CA, 0xC0400000, 0x0004002B, 0x00000006, 0x000000CC, 0x3CA3D70A,
    0x0005002C, 0x00000024, 0x000000D7, 0x00000053, 0x00000053, 0x0004003B, 0x0000003F, 0x000000E9,
    0x00000000, 0x0004002B, 0x00000028, 0x000000EE, 0x00000000, 0x0005002C, 0x0000005E, 0x000000EF,
    0x000000EE, 0x00000061, 0x0005002C, 0x0000005E, 0x000000F1, 0x000000EE, 0x000000EE, 0x0005002C,
    0x00000024, 0x000000FB, 0x00000049, 0x0000000E, 0x0005002C, 0x0000005E, 0x00000131, 0x00000061,
    0x000000EE, 0x0005002C, 0x00000024, 0x0000013C, 0x0000000E, 0x00000049, 0x0004002B, 0x00000028,
    0x00000174, 0xFFFFFFFF, 0x0005002C, 0x0000005E, 0x00000175, 0x00000174, 0x000000EE, 0x0004002B,
    0x00000006, 0x00000180, 0xBF800000, 0x0005002C, 0x00000024, 0x00000181, 0x00000180, 0x00000049,
    0x0005002C, 0x0000005E, 0x000001F9, 0x000000EE, 0x00000174, 0x0005002C, 0x00000024, 0x00000204,
    0x00000049, 0x00000180, 0x0004002B, 0x00000006, 0x0000035B, 0x3D99999A, 0x0006002C, 0x00000083,
    0x0000035C, 0x0000035B, 0x0000035B, 0x0000035B, 0x0004002B, 0x00000006, 0x00000369, 0x3EAAAAAB,
    0x0004002B, 0x00000006, 0x00000373, 0x41200000, 0x0004002B, 0x00000006, 0x0000037F, 0x3D4CCCCD,
    0x0004002B, 0x00000028, 0x000003A8, 0x0000000C, 0x0004002B, 0x00000006, 0x000003B2, 0x358637BD,
    0x00040017, 0x000003B9, 0x0000004E, 0x00000003, 0x0004002B, 0x00000006, 0x000003CE, 0x3DCCCCCD,
    0x0004002B, 0x00000006, 0x000003D6, 0x33FFFAE5, 0x0004002B, 0x00000028, 0x000003E1, 0x0000000D,
    0x00040020, 0x000003F4, 0x00000003, 0x00000020, 0x0004003B, 0x000003F4, 0x000003F5, 0x00000003,
    0x0003002E, 0x00000083, 0x00000487, 0x0004002B, 0x00000006, 0x0000048C, 0x40555556, 0x00050036,
    0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200F8, 0x00000005, 0x00060041, 0x0000002B,
    0x0000002C, 0x00000027, 0x00000029, 0x0000002A, 0x0004003D, 0x00000006, 0x0000002D, 0x0000002C,
    0x00060041, 0x0000002B, 0x00000030, 0x00000027, 0x00000029, 0x0000002F, 0x0004003D, 0x00000006,
    0x00000031, 0x00000030, 0x0004003D, 0x00000020, 0x00000036, 0x00000035, 0x0007004F, 0x00000024,
    0x00000037, 0x00000036, 0x00000036, 0x00000000, 0x00000001, 0x00050041, 0x00000039, 0x0000003A,
    0x00000027, 0x00000038, 0x0004003D, 0x00000024, 0x0000003B, 0x0000003A, 0x00050085, 0x00000024,
    0x0000003C, 0x00000037, 0x0000003B, 0x0004003D, 0x0000003E, 0x00000041, 0x00000040, 0x0004003D,
    0x00000042, 0x00000045, 0x00000044, 0x00050056, 0x00000046, 0x00000047, 0x00000041, 0x00000045,
    0x00070058, 0x00000020, 0x0000004A, 0x00000047, 0x0000003C, 0x00000002, 0x00000049, 0x00050051,
    0x00000006, 0x0000004C, 0x0000004A, 0x00000003, 0x000500BA, 0x0000004E, 0x0000004F, 0x0000004C,
    0x0000004D, 0x000600A9, 0x00000006, 0x00000050, 0x0000004F, 0x0000000E, 0x00000049, 0x000500BA,
    0x0000004E, 0x00000054, 0x00000050, 0x00000053, 0x000300F7, 0x00000057, 0x00000000, 0x000400FA,
    0x00000054, 0x00000056, 0x00000059, 0x000200F8, 0x00000059, 0x00050041, 0x00000039, 0x0000005B,
    0x00000027, 0x0000005A, 0x0004003D, 0x00000024, 0x0000005C, 0x0000005B, 0x000200F9, 0x00000057,
    0x000200F8, 0x00000056, 0x000200F9, 0x00000057, 0x000200F8, 0x00000057, 0x000700F5, 0x00000024,
    0x00000478, 0x0000005C, 0x00000059, 0x00000058, 0x00000056, 0x00050041, 0x00000039, 0x00000062,
    0x00000027, 0x00000061, 0x0004003D, 0x00000024, 0x00000063, 0x00000062, 0x0004006E, 0x0000005E,
    0x00000064, 0x00000063, 0x00050082, 0x0000005E, 0x00000066, 0x00000064, 0x00000065, 0x00050051,
    0x00000006, 0x00000069, 0x0000003C, 0x00000000, 0x00050051, 0x00000006, 0x0000006B, 0x00000478,
    0x00000000, 0x00060041, 0x0000002B, 0x0000006D, 0x00000027, 0x0000006C, 0x0000002A, 0x0004003D,
    0x00000006, 0x0000006E, 0x0000006D, 0x0008000C, 0x00000006, 0x00000070, 0x00000001, 0x00000032,
    0x0000006B, 0x0000006E, 0x00000069, 0x0008000C, 0x00000006, 0x00000071, 0x00000001, 0x0000002B,
    0x00000070, 0x00000049, 0x0000000E, 0x00050051, 0x00000006, 0x00000074, 0x0000003C, 0x00000001,
    0x00050051, 0x00000006, 0x00000076, 0x00000478, 0x00000001, 0x00060041, 0x0000002B, 0x00000077,
    0x00000027, 0x0000006C, 0x0000002F, 0x0004003D, 0x00000006, 0x00000078, 0x00000077, 0x0008000C,
    0x00000006, 0x0000007A, 0x00000001, 0x00000032, 0x00000076, 0x00000078, 0x00000074, 0x0008000C,
    0x00000006, 0x0000007B, 0x00000001, 0x0000002B, 0x0000007A, 0x00000049, 0x0000000E, 0x00050050,
    0x00000024, 0x00000486, 0x00000071, 0x0000007B, 0x00050085, 0x00000024, 0x00000081, 0x00000486,
    0x00000063, 0x0004006E, 0x0000005E, 0x00000082, 0x00000081, 0x00050056, 0x00000046, 0x00000088,
    0x00000041, 0x00000045, 0x00070058, 0x00000020, 0x0000008A, 0x00000088, 0x00000486, 0x00000002,
    0x00000049, 0x0007004F, 0x00000024, 0x0000008E, 0x0000008A, 0x00000487, 0x00000000, 0x00000001,
    0x00050051, 0x00000006, 0x00000092, 0x0000008A, 0x00000000, 0x0008000C, 0x00000006, 0x00000096,
    0x00000001, 0x00000032, 0x00000090, 0x00000092, 0x00000069, 0x0008000C, 0x00000006, 0x00000097,
    0x00000001, 0x0000002B, 0x00000096, 0x00000049, 0x0000000E, 0x00050051, 0x00000006, 0x0000009A,
    0x0000008A, 0x00000001, 0x0008000C, 0x00000006, 0x0000009E, 0x00000001, 0x00000032, 0x00000053,
    0x0000009A, 0x00000074, 0x0008000C, 0x00000006, 0x0000009F, 0x00000001, 0x0000002B, 0x0000009E,
    0x00000049, 0x0000000E, 0x00050050, 0x00000024, 0x00000488, 0x00000097, 0x0000009F, 0x00050051,
    0x00000006, 0x000000A4, 0x0000008A, 0x00000002, 0x0004003D, 0x0000003E, 0x000000A7, 0x000000A6,
    0x00050056, 0x00000046, 0x000000A9, 0x000000A7, 0x00000045, 0x00070058, 0x00000020, 0x000000AB,
    0x000000A9, 0x00000488, 0x00000002, 0x00000049, 0x0008004F, 0x00000083, 0x000000AC, 0x000000AB,
    0x000000AB, 0x00000000, 0x00000001, 0x00000002, 0x0008000C, 0x00000006, 0x000000B6, 0x00000001,
    0x00000032, 0x000000B3, 0x0000002D, 0x000000B3, 0x0007000C, 0x00000006, 0x000000B7, 0x00000001,
    0x00000028, 0x0000000E, 0x000000B6, 0x00050085, 0x00000006, 0x000000BB, 0x000000B9, 0x000000A4,
    0x0008000C, 0x00000006, 0x000000C0, 0x00000001, 0x0000002E, 0x0000002D, 0x000000B7, 0x000000BB,
    0x00050041, 0x00000039, 0x000000C4, 0x00000027, 0x000000C3, 0x0004003D, 0x00000024, 0x000000C5,
    0x000000C4, 0x00050085, 0x00000024, 0x000000C6, 0x0000008E, 0x000000C5, 0x0006000C, 0x00000006,
    0x000000C7, 0x00000001, 0x00000042, 0x000000C6, 0x00050085, 0x00000006, 0x000000CD, 0x000000C7,
    0x000000CC, 0x0008000C, 0x00000006, 0x000000CE, 0x00000001, 0x0000002B, 0x000000CD, 0x00000049,
    0x0000000E, 0x0008000C, 0x00000006, 0x000000CF, 0x00000001, 0x0000002E, 0x000000C9, 0x000000CA,
    0x000000CE, 0x0004006F, 0x00000024, 0x000000D6, 0x00000082, 0x00050081, 0x00000024, 0x000000D8,
    0x000000D6, 0x000000D7, 0x00050083, 0x00000024, 0x000000DA, 0x000000D8, 0x00000478, 0x00050085,
    0x00000006, 0x000000DC, 0x000000C0, 0x00000053, 0x00050085, 0x00000006, 0x000000E0, 0x000000DC,
    0x000000DC, 0x0004007F, 0x00000024, 0x00000489, 0x0000003C, 0x0008000C, 0x00000024, 0x000000E7,
    0x00000001, 0x00000032, 0x00000489, 0x00000063, 0x000000DA, 0x0004003D, 0x0000003E, 0x000000EA,
    0x000000E9, 0x00050056, 0x00000046, 0x000000EC, 0x000000EA, 0x00000045, 0x00050080, 0x0000005E,
    0x000000F0, 0x00000082, 0x000000EF, 0x0008000C, 0x0000005E, 0x000000F3, 0x00000001, 0x0000002D,
    0x000000F0, 0x000000F1, 0x00000066, 0x00040064, 0x0000003E, 0x000000F4, 0x000000EC, 0x0007005F,
    0x00000020, 0x000000F5, 0x000000F4, 0x000000F3, 0x00000002, 0x000000EE, 0x0008004F, 0x00000083,
    0x000000F6, 0x000000F5, 0x000000F5, 0x00000000, 0x00000001, 0x00000002, 0x00050081, 0x00000024,
    0x000000FC, 0x000000E7, 0x000000FB, 0x00050094, 0x00000006, 0x00000100, 0x000000FC, 0x000000FC,
    0x00050085, 0x00000006, 0x00000104, 0x00000100, 0x000000E0, 0x0008000C, 0x00000006, 0x00000105,
    0x00000001, 0x0000002B, 0x00000104, 0x00000049, 0x0000000E, 0x00050083, 0x00000006, 0x00000402,
    0x00000105, 0x0000000E, 0x00050085, 0x00000006, 0x00000405, 0x00000402, 0x00000402, 0x0008000C,
    0x00000006, 0x00000409, 0x00000001, 0x00000032, 0x00000015, 0x00000402, 0x00000405, 0x00050085,
    0x00000006, 0x0000040C, 0x00000409, 0x00000405, 0x0005008E, 0x00000083, 0x0000010C, 0x000000F6,
    0x0000040C, 0x00050051, 0x00000006, 0x0000010E, 0x0000010C, 0x00000000, 0x00050051, 0x00000006,
    0x0000010F, 0x0000010C, 0x00000001, 0x00050051, 0x00000006, 0x00000110, 0x0000010C, 0x00000002,
    0x00070050, 0x00000020, 0x00000111, 0x0000010E, 0x0000010F, 0x00000110, 0x0000040C, 0x00050085,
    0x00000006, 0x00000117, 0x00000100, 0x000000CF, 0x0006000C, 0x00000006, 0x00000118, 0x00000001,
    0x0000001B, 0x00000117, 0x0005008E, 0x00000083, 0x00000120, 0x000000F6, 0x00000118, 0x00050056,
    0x00000046, 0x0000012F, 0x000000EA, 0x00000045, 0x00050080, 0x0000005E, 0x00000132, 0x00000082,
    0x00000131, 0x0008000C, 0x0000005E, 0x00000134, 0x00000001, 0x0000002D, 0x00000132, 0x000000F1,
    0x00000066, 0x00040064, 0x0000003E, 0x00000135, 0x0000012F, 0x0007005F, 0x00000020, 0x00000136,
    0x00000135, 0x00000134, 0x00000002, 0x000000EE, 0x0008004F, 0x00000083, 0x00000137, 0x00000136,
    0x00000136, 0x00000000, 0x00000001, 0x00000002, 0x00050081, 0x00000024, 0x0000013D, 0x000000E7,
    0x0000013C, 0x00050094, 0x00000006, 0x00000141, 0x0000013D, 0x0000013D, 0x00050085, 0x00000006,
    0x00000145, 0x00000141, 0x000000E0, 0x0008000C, 0x00000006, 0x00000146, 0x00000001, 0x0000002B,
    0x00000145, 0x00000049, 0x0000000E, 0x00050083, 0x00000006, 0x00000413, 0x00000146, 0x0000000E,
    0x00050085, 0x00000006, 0x00000416, 0x00000413, 0x00000413, 0x0008000C, 0x00000006, 0x0000041A,
    0x00000001, 0x00000032, 0x00000015, 0x00000413, 0x00000416, 0x00050085, 0x00000006, 0x0000041D,
    0x0000041A, 0x00000416, 0x0005008E, 0x00000083, 0x0000014D, 0x00000137, 0x0000041D, 0x00050051,
    0x00000006, 0x0000014F, 0x0000014D, 0x00000000, 0x00050051, 0x00000006, 0x00000150, 0x0000014D,
    0x00000001, 0x00050051, 0x00000006, 0x00000151, 0x0000014D, 0x00000002, 0x00070050, 0x00000020,
    0x00000152, 0x0000014F, 0x00000150, 0x00000151, 0x0000041D, 0x00050081, 0x00000020, 0x00000154,
    0x00000111, 0x00000152, 0x00050085, 0x00000006, 0x00000158, 0x00000141, 0x000000CF, 0x0006000C,
    0x00000006, 0x00000159, 0x00000001, 0x0000001B, 0x00000158, 0x0007000C, 0x00000083, 0x0000015C,
    0x00000001, 0x00000025, 0x000000F6, 0x00000137, 0x0007000C, 0x00000083, 0x0000015F, 0x00000001,
    0x00000028, 0x000000F6, 0x00000137, 0x0005008E, 0x00000083, 0x00000163, 0x00000137, 0x00000159,
    0x00050081, 0x00000083, 0x00000166, 0x00000120, 0x00000163, 0x00050085, 0x00000083, 0x00000169,
    0x00000137, 0x00000163, 0x0008000C, 0x00000083, 0x0000016B, 0x00000001, 0x00000032, 0x000000F6,
    0x00000120, 0x00000169, 0x00050081, 0x00000006, 0x0000016E, 0x00000118, 0x00000159, 0x00050056,
    0x00000046, 0x00000172, 0x000000EA, 0x00000045, 0x00050080, 0x0000005E, 0x00000176, 0x00000082,
    0x00000175, 0x0008000C, 0x0000005E, 0x00000178, 0x00000001, 0x0000002D, 0x00000176, 0x000000F1,
    0x00000066, 0x00040064, 0x0000003E, 0x00000179, 0x00000172, 0x0007005F, 0x00000020, 0x0000017A,
    0x00000179, 0x00000178, 0x00000002, 0x000000EE, 0x0008004F, 0x00000083, 0x0000017B, 0x0000017A,
    0x0000017A, 0x00000000, 0x00000001, 0x00000002, 0x00050081, 0x00000024, 0x00000182, 0x000000E7,
    0x00000181, 0x00050094, 0x00000006, 0x00000186, 0x00000182, 0x00000182, 0x00050085, 0x00000006,
    0x0000018A, 0x00000186, 0x000000E0, 0x0008000C, 0x00000006, 0x0000018B, 0x00000001, 0x0000002B,
    0x0000018A, 0x00000049, 0x0000000E, 0x00050083, 0x00000006, 0x00000424, 0x0000018B, 0x0000000E,
    0x00050085, 0x00000006, 0x00000427, 0x00000424, 0x00000424, 0x0008000C, 0x00000006, 0x0000042B,
    0x00000001, 0x00000032, 0x00000015, 0x00000424, 0x00000427, 0x00050085, 0x00000006, 0x0000042E,
    0x0000042B, 0x00000427, 0x0005008E, 0x00000083, 0x00000192, 0x0000017B, 0x0000042E, 0x00050051,
    0x00000006, 0x00000194, 0x00000192, 0x00000000, 0x00050051, 0x00000006, 0x00000195, 0x00000192,
    0x00000001, 0x00050051, 0x00000006, 0x00000196, 0x00000192, 0x00000002, 0x00070050, 0x00000020,
    0x00000197, 0x00000194, 0x00000195, 0x00000196, 0x0000042E, 0x00050081, 0x00000020, 0x00000199,
    0x00000154, 0x00000197, 0x00050085, 0x00000006, 0x0000019D, 0x00000186, 0x000000CF, 0x0006000C,
    0x00000006, 0x0000019E, 0x00000001, 0x0000001B, 0x0000019D, 0x0007000C, 0x00000083, 0x000001A1,
    0x00000001, 0x00000025, 0x0000015C, 0x0000017B, 0x0007000C, 0x00000083, 0x000001A4, 0x00000001,
    0x00000028, 0x0000015F, 0x0000017B, 0x0005008E, 0x00000083, 0x000001A8, 0x0000017B, 0x0000019E,
    0x00050081, 0x00000083, 0x000001AB, 0x00000166, 0x000001A8, 0x0008000C, 0x00000083, 0x000001B0,
    0x00000001, 0x00000032, 0x0000017B, 0x000001A8, 0x0000016B, 0x00050081, 0x00000006, 0x000001B3,
    0x0000016E, 0x0000019E, 0x00050056, 0x00000046, 0x000001B7, 0x000000EA, 0x00000045, 0x0008000C,
    0x0000005E, 0x000001BB, 0x00000001, 0x0000002D, 0x00000082, 0x000000F1, 0x00000066, 0x00040064,
    0x0000003E, 0x000001BC, 0x000001B7, 0x0007005F, 0x00000020, 0x000001BD, 0x000001BC, 0x000001BB,
    0x00000002, 0x000000EE, 0x0008004F, 0x00000083, 0x000001BE, 0x000001BD, 0x000001BD, 0x00000000,
    0x00000001, 0x00000002, 0x00050094, 0x00000006, 0x000001C6, 0x000000E7, 0x000000E7, 0x00050085,
    0x00000006, 0x000001CA, 0x000001C6, 0x000000E0, 0x0008000C, 0x00000006, 0x000001CB, 0x00000001,
    0x0000002B, 0x000001CA, 0x00000049, 0x0000000E, 0x00050083, 0x00000006, 0x00000435, 0x000001CB,
    0x0000000E, 0x00050085, 0x00000006, 0x00000438, 0x00000435, 0x00000435, 0x0008000C, 0x00000006,
    0x0000043C, 0x00000001, 0x00000032, 0x00000015, 0x00000435, 0x00000438, 0x00050085, 0x00000006,
    0x0000043F, 0x0000043C, 0x00000438, 0x0005008E, 0x00000083, 0x000001D2, 0x000001BE, 0x0000043F,
    0x00050051, 0x00000006, 0x000001D4, 0x000001D2, 0x00000000, 0x00050051, 0x00000006, 0x000001D5,
    0x000001D2, 0x00000001, 0x00050051, 0x00000006, 0x000001D6, 0x000001D2, 0x00000002, 0x00070050,
    0x00000020, 0x000001D7, 0x000001D4, 0x000001D5, 0x000001D6, 0x0000043F, 0x00050081, 0x00000020,
    0x000001D9, 0x00000199, 0x000001D7, 0x00050085, 0x00000006, 0x000001DD, 0x000001C6, 0x000000CF,
    0x0006000C, 0x00000006, 0x000001DE, 0x00000001, 0x0000001B, 0x000001DD, 0x0007000C, 0x00000083,
    0x000001E1, 0x00000001, 0x00000025, 0x000001A1, 0x000001BE, 0x0007000C, 0x00000083, 0x000001E4,
    0x00000001, 0x00000028, 0x000001A4, 0x000001BE, 0x0005008E, 0x00000083, 0x000001E8, 0x000001BE,
    0x000001DE, 0x00050081, 0x00000083, 0x000001EB, 0x000001AB, 0x000001E8, 0x0008000C, 0x00000083,
    0x000001F0, 0x00000001, 0x00000032, 0x000001BE, 0x000001E8, 0x000001B0, 0x00050081, 0x00000006,
    0x000001F3, 0x000001B3, 0x000001DE, 0x00050056, 0x00000046, 0x000001F7, 0x000000EA, 0x00000045,
    0x00050080, 0x0000005E, 0x000001FA, 0x00000082, 0x000001F9, 0x0008000C, 0x0000005E, 0x000001FC,
    0x00000001, 0x0000002D, 0x000001FA, 0x000000F1, 0x00000066, 0x00040064, 0x0000003E, 0x000001FD,
    0x000001F7, 0x0007005F, 0x00000020, 0x000001FE, 0x000001FD, 0x000001FC, 0x00000002, 0x000000EE,
    0x0008004F, 0x00000083, 0x000001FF, 0x000001FE, 0x000001FE, 0x00000000, 0x00000001, 0x00000002,
    0x00050081, 0x00000024, 0x00000205, 0x000000E7, 0x00000204, 0x00050094, 0x00000006, 0x00000209,
    0x00000205, 0x00000205, 0x00050085, 0x00000006, 0x0000020D, 0x00000209, 0x000000E0, 0x0008000C,
    0x00000006, 0x0000020E, 0x00000001, 0x0000002B, 0x0000020D, 0x00000049, 0x0000000E, 0x00050083,
    0x00000006, 0x00000446, 0x0000020E, 0x0000000E, 0x00050085, 0x00000006, 0x00000449, 0x00000446,
    0x00000446, 0x0008000C, 0x00000006, 0x0000044D, 0x00000001, 0x00000032, 0x00000015, 0x00000446,
    0x00000449, 0x00050085, 0x00000006, 0x00000450, 0x0000044D, 0x00000449, 0x0005008E, 0x00000083,
    0x00000215, 0x000001FF, 0x00000450, 0x00050051, 0x00000006, 0x00000217, 0x00000215, 0x00000000,
    0x00050051, 0x00000006, 0x00000218, 0x00000215, 0x00000001, 0x00050051, 0x00000006, 0x00000219,
    0x00000215, 0x00000002, 0x00070050, 0x00000020, 0x0000021A, 0x00000217, 0x00000218, 0x00000219,
    0x00000450, 0x00050081, 0x00000020, 0x0000021C, 0x000001D9, 0x0000021A, 0x00050085, 0x00000006,
    0x00000220, 0x00000209, 0x000000CF, 0x0006000C, 0x00000006, 0x00000221, 0x00000001, 0x0000001B,
    0x00000220, 0x0007000C, 0x00000083, 0x00000224, 0x00000001, 0x00000025, 0x000001E1, 0x000001FF,
    0x0007000C, 0x00000083, 0x00000227, 0x00000001, 0x00000028, 0x000001E4, 0x000001FF, 0x0005008E,
    0x00000083, 0x0000022B, 0x000001FF, 0x00000221, 0x00050081, 0x00000083, 0x0000022E, 0x000001EB,
    0x0000022B, 0x0008000C, 0x00000083, 0x00000233, 0x00000001, 0x00000032, 0x000001FF, 0x0000022B,
    0x000001F0, 0x00050081, 0x00000006, 0x00000236, 0x000001F3, 0x00000221, 0x00050088, 0x00000006,
    0x00000346, 0x0000000E, 0x00000236, 0x0005008E, 0x00000083, 0x00000349, 0x0000022E, 0x00000346,
    0x0005008E, 0x00000083, 0x0000034C, 0x00000233, 0x00000346, 0x0004007F, 0x00000083, 0x0000048A,
    0x00000349, 0x0008000C, 0x00000083, 0x00000351, 0x00000001, 0x00000032, 0x0000048A, 0x00000349,
    0x0000034C, 0x0006000C, 0x00000083, 0x00000352, 0x00000001, 0x00000004, 0x00000351, 0x0006000C,
    0x00000083, 0x00000353, 0x00000001, 0x0000001F, 0x00000352, 0x0008004F, 0x00000083, 0x00000355,
    0x0000021C, 0x0000021C, 0x00000000, 0x00000001, 0x00000002, 0x00050051, 0x00000006, 0x00000357,
    0x0000021C, 0x00000003, 0x00060050, 0x00000083, 0x00000358, 0x00000357, 0x00000357, 0x00000357,
    0x00050088, 0x00000083, 0x00000359, 0x00000355, 0x00000358, 0x00050083, 0x00000083, 0x0000035D,
    0x00000224, 0x0000035C, 0x00050081, 0x00000083, 0x0000035F, 0x00000227, 0x0000035C, 0x0008000C,
    0x00000083, 0x00000360, 0x00000001, 0x0000002B, 0x00000359, 0x0000035D, 0x0000035F, 0x00050051,
    0x00000006, 0x00000362, 0x00000360, 0x00000000, 0x00050051, 0x00000006, 0x00000364, 0x00000360,
    0x00000001, 0x00050051, 0x00000006, 0x00000366, 0x00000360, 0x00000002, 0x00050085, 0x00000006,
    0x0000036A, 0x00000357, 0x00000369, 0x00070050, 0x00000020, 0x0000048B, 0x00000362, 0x00000364,
    0x00000366, 0x0000036A, 0x00050083, 0x00000006, 0x0000036E, 0x0000000E, 0x000000A4, 0x00050085,
    0x00000006, 0x00000374, 0x00000357, 0x0000048C, 0x00050085, 0x00000006, 0x00000376, 0x00000373,
    0x000000C7, 0x0008000C, 0x00000006, 0x00000377, 0x00000001, 0x0000002B, 0x00000376, 0x00000049,
    0x0000000E, 0x0008000C, 0x00000006, 0x00000378, 0x00000001, 0x0000002E, 0x0000036E, 0x00000374,
    0x00000377, 0x0007000C, 0x00000006, 0x00000379, 0x00000001, 0x00000025, 0x0000036E, 0x00000378,
    0x00050085, 0x00000006, 0x00000380, 0x000000C7, 0x0000037F, 0x0008000C, 0x00000006, 0x00000381,
    0x00000001, 0x0000002B, 0x00000380, 0x00000049, 0x0000000E, 0x0008000C, 0x00000006, 0x00000382,
    0x00000001, 0x0000002E, 0x00000379, 0x0000036A, 0x00000381, 0x0007000C, 0x00000006, 0x00000383,
    0x00000001, 0x00000025, 0x00000379, 0x00000382, 0x0007000C, 0x00000006, 0x0000038B, 0x00000001,
    0x00000028, 0x000000A4, 0x00000381, 0x0008000C, 0x00000006, 0x0000038F, 0x00000001, 0x0000002E,
    0x00000031, 0x0000000E, 0x0000038B, 0x0005008E, 0x00000083, 0x00000393, 0x00000353, 0x0000038F,
    0x00050083, 0x00000083, 0x00000397, 0x00000349, 0x00000393, 0x00050081, 0x00000083, 0x0000039B,
    0x00000349, 0x00000393, 0x0007000C, 0x00000083, 0x0000039E, 0x00000001, 0x00000025, 0x00000227,
    0x0000039B, 0x0007000C, 0x00000083, 0x000003A1, 0x00000001, 0x00000028, 0x00000224, 0x00000397,
    0x0008000C, 0x00000083, 0x000003A6, 0x00000001, 0x0000002B, 0x000000AC, 0x000003A1, 0x0000039E,
    0x00050041, 0x0000002B, 0x000003A9, 0x00000027, 0x000003A8, 0x0004003D, 0x00000006, 0x000003AA,
    0x000003A9, 0x0006000C, 0x00000006, 0x000003AD, 0x00000001, 0x00000004, 0x00000092, 0x0006000C,
    0x00000006, 0x000003B0, 0x00000001, 0x00000004, 0x0000009A, 0x00050081, 0x00000006, 0x000003B1,
    0x000003AD, 0x000003B0, 0x000500BA, 0x0000004E, 0x000003B3, 0x000003B1, 0x000003B2, 0x000600A9,
    0x00000006, 0x0000048D, 0x000003B3, 0x00000049, 0x000003AA, 0x000500BA, 0x000003B9, 0x000003BA,
    0x000003A1, 0x000000AC, 0x0004009A, 0x0000004E, 0x000003BB, 0x000003BA, 0x000400A8, 0x0000004E,
    0x000003BC, 0x000003BB, 0x000300F7, 0x000003BE, 0x00000000, 0x000400FA, 0x000003BC, 0x000003BD,
    0x000003BE, 0x000200F8, 0x000003BD, 0x000500BA, 0x000003B9, 0x000003C1, 0x000000AC, 0x0000039E,
    0x0004009A, 0x0000004E, 0x000003C2, 0x000003C1, 0x000200F9, 0x000003BE, 0x000200F8, 0x000003BE,
    0x000700F5, 0x0000004E, 0x000003C3, 0x000003BB, 0x00000057, 0x000003C2, 0x000003BD, 0x000600A9,
    0x00000006, 0x000003C5, 0x000003C3, 0x0000048D, 0x0000000E, 0x0008000C, 0x00000006, 0x000003C9,
    0x00000001, 0x0000002B, 0x000003C5, 0x00000049, 0x0000000E, 0x00060050, 0x00000083, 0x000003CA,
    0x000003C9, 0x000003C9, 0x000003C9, 0x0008000C, 0x00000083, 0x000003CB, 0x00000001, 0x0000002E,
    0x000003A6, 0x000000AC, 0x000003CA, 0x0007000C, 0x00000006, 0x000003CF, 0x00000001, 0x00000025,
    0x00000383, 0x000003CE, 0x0008000C, 0x00000006, 0x000003D4, 0x00000001, 0x0000002E, 0x000003CF,
    0x00000383, 0x000003C9, 0x0008000C, 0x00000006, 0x000003DA, 0x00000001, 0x00000032, 0x00000357,
    0x00000369, 0x000003D4, 0x0007000C, 0x00000006, 0x000003DB, 0x00000001, 0x00000028, 0x000003D6,
    0x000003DA, 0x00050088, 0x00000006, 0x000003E0, 0x0000036A, 0x000003DB, 0x00050041, 0x0000002B,
    0x000003E2, 0x00000027, 0x000003E1, 0x0004003D, 0x00000006, 0x000003E3, 0x000003E2, 0x00050081,
    0x00000006, 0x000003E4, 0x000003E0, 0x000003E3, 0x00050081, 0x00000006, 0x000003E6, 0x000003E4,
    0x00000050, 0x0008000C, 0x00000006, 0x000003E7, 0x00000001, 0x0000002B, 0x000003E6, 0x00000049,
    0x0000000E, 0x0008004F, 0x00000083, 0x000003EA, 0x0000048B, 0x0000048B, 0x00000000, 0x00000001,
    0x00000002, 0x00060050, 0x00000083, 0x000003EC, 0x000003E7, 0x000003E7, 0x000003E7, 0x0008000C,
    0x00000083, 0x000003ED, 0x00000001, 0x0000002E, 0x000003CB, 0x000003EA, 0x000003EC, 0x00050051,
    0x00000006, 0x000003EF, 0x000003ED, 0x00000000, 0x00050051, 0x00000006, 0x000003F1, 0x000003ED,
    0x00000001, 0x00050051, 0x00000006, 0x000003F3, 0x000003ED, 0x00000002, 0x00070050, 0x00000020,
    0x000003FB, 0x000003EF, 0x000003F1, 0x000003F3, 0x0000000E, 0x0003003E, 0x000003F5, 0x000003FB,
    0x000100FD, 0x00010038,
};

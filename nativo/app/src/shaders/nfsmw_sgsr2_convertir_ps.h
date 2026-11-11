// Generated with tools/compilar_shader_presentador.py from app/src/shaders/nfsmw_sgsr2_convertir.frag.
#if 0
; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 10
; Bound: 487
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %gl_FragCoord %417
               OpExecutionMode %4 OriginUpperLeft
               OpDecorate %gl_FragCoord BuiltIn FragCoord
               OpDecorate %_arr_v4float_uint_4 ArrayStride 16
               OpMemberDecorate %_struct_18 0 Offset 0
               OpMemberDecorate %_struct_18 1 Offset 64
               OpMemberDecorate %_struct_18 2 Offset 72
               OpMemberDecorate %_struct_18 3 Offset 80
               OpMemberDecorate %_struct_18 4 Offset 88
               OpMemberDecorate %_struct_18 5 Offset 96
               OpMemberDecorate %_struct_18 6 Offset 104
               OpMemberDecorate %_struct_18 7 Offset 112
               OpMemberDecorate %_struct_18 8 Offset 120
               OpMemberDecorate %_struct_18 9 Offset 128
               OpMemberDecorate %_struct_18 10 Offset 136
               OpMemberDecorate %_struct_18 11 Offset 144
               OpMemberDecorate %_struct_18 12 Offset 148
               OpMemberDecorate %_struct_18 13 Offset 152
               OpMemberDecorate %_struct_18 14 Offset 156
               OpMemberDecorate %_struct_18 15 Offset 160
               OpMemberDecorate %_struct_18 16 Offset 164
               OpMemberDecorate %_struct_18 17 Offset 168
               OpMemberDecorate %_struct_18 18 Offset 172
               OpDecorate %_struct_18 Block
               OpDecorate %20 DescriptorSet 0
               OpDecorate %20 Binding 0
               OpDecorate %56 DescriptorSet 0
               OpDecorate %56 Binding 1
               OpDecorate %60 DescriptorSet 0
               OpDecorate %60 Binding 4
               OpDecorate %373 DescriptorSet 0
               OpDecorate %373 Binding 2
               OpDecorate %382 DescriptorSet 0
               OpDecorate %382 Binding 3
               OpDecorate %417 Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v4float = OpTypeVector %float 4
%_ptr_Input_v4float = OpTypePointer Input %v4float
%gl_FragCoord = OpVariable %_ptr_Input_v4float Input
       %uint = OpTypeInt 32 0
     %uint_4 = OpConstant %uint 4
%_arr_v4float_uint_4 = OpTypeArray %v4float %uint_4
 %_struct_18 = OpTypeStruct %_arr_v4float_uint_4 %v2float %v2float %v2float %v2float %v2float %v2float %v2float %v2float %v2float %v2float %float %float %float %float %float %float %float %float
%_ptr_Uniform__struct_18 = OpTypePointer Uniform %_struct_18
         %20 = OpVariable %_ptr_Uniform__struct_18 Uniform
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
%_ptr_Uniform_v2float = OpTypePointer Uniform %v2float
      %v2int = OpTypeVector %int 2
      %int_7 = OpConstant %int 7
      %int_8 = OpConstant %int 8
  %float_0_5 = OpConstant %float 0.5
      %int_9 = OpConstant %int 9
         %54 = OpTypeImage %float 2D 0 0 0 1 Unknown
%_ptr_UniformConstant_54 = OpTypePointer UniformConstant %54
         %56 = OpVariable %_ptr_UniformConstant_54 UniformConstant
         %58 = OpTypeSampler
%_ptr_UniformConstant_58 = OpTypePointer UniformConstant %58
         %60 = OpVariable %_ptr_UniformConstant_58 UniformConstant
         %62 = OpTypeSampledImage %54
      %int_0 = OpConstant %int 0
     %uint_0 = OpConstant %uint 0
%_ptr_Uniform_float = OpTypePointer Uniform %float
    %float_2 = OpConstant %float 2
    %float_0 = OpConstant %float 0
     %uint_1 = OpConstant %uint 1
     %int_14 = OpConstant %int 14
       %bool = OpTypeBool
    %float_1 = OpConstant %float 1
        %122 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
%float_0_999989986 = OpConstant %float 0.999989986
%float_1_36999997en05 = OpConstant %float 1.36999997e-05
     %int_11 = OpConstant %int 11
      %int_1 = OpConstant %int 1
%float_1_18999999en07 = OpConstant %float 1.18999999e-07
 %float_0_25 = OpConstant %float 0.25
    %v3float = OpTypeVector %float 3
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
      %int_2 = OpConstant %int 2
    %float_4 = OpConstant %float 4
        %364 = OpConstantComposite %v2float %float_0 %float_0
     %int_16 = OpConstant %int 16
        %373 = OpVariable %_ptr_UniformConstant_54 UniformConstant
        %382 = OpVariable %_ptr_UniformConstant_54 UniformConstant
     %int_17 = OpConstant %int 17
     %int_15 = OpConstant %int 15
%_ptr_Output_v4float = OpTypePointer Output %v4float
        %417 = OpVariable %_ptr_Output_v4float Output
        %478 = OpConstantNull %v3float
 %float_n0_5 = OpConstant %float -0.5
        %480 = OpConstantComposite %v2float %float_n0_5 %float_n0_5
   %float_n1 = OpConstant %float -1
   %float_n2 = OpConstant %float -2
     %v2bool = OpTypeVector %bool 2
          %4 = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpLoad %v4float %gl_FragCoord
         %14 = OpVectorShuffle %v2float %13 %13 0 1
         %24 = OpAccessChain %_ptr_Uniform_v2float %20 %int_3
         %25 = OpLoad %v2float %24
         %26 = OpFMul %v2float %14 %25
         %32 = OpConvertFToS %v2int %14
         %36 = OpAccessChain %_ptr_Uniform_v2float %20 %int_7
         %37 = OpLoad %v2float %36
         %40 = OpAccessChain %_ptr_Uniform_v2float %20 %int_8
         %41 = OpLoad %v2float %40
         %42 = OpExtInst %v2float %1 Fma %26 %37 %41
         %48 = OpAccessChain %_ptr_Uniform_v2float %20 %int_9
         %49 = OpLoad %v2float %48
         %51 = OpExtInst %v2float %1 Fma %480 %49 %42
         %57 = OpLoad %54 %56
         %61 = OpLoad %58 %60
         %63 = OpSampledImage %62 %57 %61
         %66 = OpImageGather %v4float %63 %51 %int_0
         %70 = OpAccessChain %_ptr_Uniform_float %20 %int_9 %uint_0
         %71 = OpLoad %float %70
         %73 = OpFMul %float %71 %float_2
         %75 = OpCompositeConstruct %v2float %73 %float_0
         %79 = OpSampledImage %62 %57 %61
         %82 = OpFAdd %v2float %51 %75
         %83 = OpImageGather %v4float %79 %82 %int_0
         %86 = OpAccessChain %_ptr_Uniform_float %20 %int_9 %uint_1
         %87 = OpLoad %float %86
         %88 = OpFMul %float %87 %float_2
         %89 = OpCompositeConstruct %v2float %float_0 %88
         %93 = OpSampledImage %62 %57 %61
         %96 = OpFAdd %v2float %51 %89
         %97 = OpImageGather %v4float %93 %96 %int_0
        %105 = OpCompositeConstruct %v2float %73 %88
        %109 = OpSampledImage %62 %57 %61
        %112 = OpFAdd %v2float %51 %105
        %113 = OpImageGather %v4float %109 %112 %int_0
        %115 = OpAccessChain %_ptr_Uniform_float %20 %int_14
        %116 = OpLoad %float %115
        %118 = OpFOrdGreaterThan %bool %116 %float_0_5
               OpSelectionMerge %120 None
               OpBranchConditional %118 %119 %120
        %119 = OpLabel
        %124 = OpFSub %v4float %122 %66
        %126 = OpFSub %v4float %122 %83
        %128 = OpFSub %v4float %122 %97
        %130 = OpFSub %v4float %122 %113
               OpBranch %120
        %120 = OpLabel
        %460 = OpPhi %v4float %113 %5 %130 %119
        %459 = OpPhi %v4float %97 %5 %128 %119
        %458 = OpPhi %v4float %83 %5 %126 %119
        %457 = OpPhi %v4float %66 %5 %124 %119
        %135 = OpCompositeExtract %float %457 2
        %138 = OpCompositeExtract %float %458 3
        %139 = OpExtInst %float %1 FMin %135 %138
        %141 = OpCompositeExtract %float %459 1
        %142 = OpExtInst %float %1 FMin %139 %141
        %144 = OpCompositeExtract %float %460 0
        %145 = OpExtInst %float %1 FMin %142 %144
        %148 = OpCompositeExtract %float %457 1
        %150 = OpCompositeExtract %float %457 0
        %151 = OpExtInst %float %1 FMin %148 %150
        %154 = OpExtInst %float %1 FMin %151 %135
        %156 = OpCompositeExtract %float %457 3
        %157 = OpExtInst %float %1 FMin %154 %156
        %160 = OpCompositeExtract %float %459 0
        %163 = OpExtInst %float %1 FMin %145 %157
        %165 = OpCompositeExtract %float %458 0
        %166 = OpExtInst %float %1 FMin %163 %165
        %167 = OpExtInst %float %1 FMin %160 %166
        %171 = OpFOrdLessThan %bool %145 %float_0_999989986
               OpSelectionMerge %173 None
               OpBranchConditional %171 %172 %173
        %172 = OpLabel
        %176 = OpCompositeExtract %float %458 1
        %179 = OpExtInst %float %1 FMin %176 %165
        %181 = OpCompositeExtract %float %458 2
        %182 = OpExtInst %float %1 FMin %179 %181
        %185 = OpExtInst %float %1 FMin %182 %138
        %191 = OpExtInst %float %1 FMin %141 %160
        %193 = OpCompositeExtract %float %459 2
        %194 = OpExtInst %float %1 FMin %191 %193
        %196 = OpCompositeExtract %float %459 3
        %197 = OpExtInst %float %1 FMin %194 %196
        %200 = OpCompositeExtract %float %460 1
        %203 = OpExtInst %float %1 FMin %200 %144
        %205 = OpCompositeExtract %float %460 2
        %206 = OpExtInst %float %1 FMin %203 %205
        %208 = OpCompositeExtract %float %460 3
        %209 = OpExtInst %float %1 FMin %206 %208
        %215 = OpAccessChain %_ptr_Uniform_float %20 %int_11
        %216 = OpLoad %float %215
        %219 = OpAccessChain %_ptr_Uniform_v2float %20 %int_1
        %220 = OpLoad %v2float %219
        %221 = OpExtInst %float %1 Length %220
        %225 = OpFMul %float %float_1_36999997en05 %216
        %227 = OpFMul %float %225 %221
        %231 = OpFSub %float %float_1 %145
        %232 = OpFMul %float %227 %231
        %238 = OpFSub %float %145 %157
        %239 = OpExtInst %float %1 FAbs %238
        %241 = OpFAdd %float %239 %float_1_18999999en07
        %242 = OpFDiv %float %232 %241
        %243 = OpExtInst %float %1 FClamp %242 %float_0 %float_1
        %249 = OpFSub %float %145 %185
        %250 = OpExtInst %float %1 FAbs %249
        %252 = OpFAdd %float %250 %float_1_18999999en07
        %253 = OpFDiv %float %232 %252
        %254 = OpExtInst %float %1 FClamp %253 %float_0 %float_1
        %256 = OpFAdd %float %243 %254
        %260 = OpFSub %float %145 %197
        %261 = OpExtInst %float %1 FAbs %260
        %263 = OpFAdd %float %261 %float_1_18999999en07
        %264 = OpFDiv %float %232 %263
        %265 = OpExtInst %float %1 FClamp %264 %float_0 %float_1
        %267 = OpFAdd %float %256 %265
        %271 = OpFSub %float %145 %209
        %272 = OpExtInst %float %1 FAbs %271
        %274 = OpFAdd %float %272 %float_1_18999999en07
        %275 = OpFDiv %float %232 %274
        %276 = OpExtInst %float %1 FClamp %275 %float_0 %float_1
        %278 = OpFAdd %float %267 %276
        %475 = OpFNegate %float %278
        %282 = OpExtInst %float %1 Fma %475 %float_0_25 %float_1
        %283 = OpExtInst %float %1 FClamp %282 %float_0 %float_1
               OpBranch %173
        %173 = OpLabel
        %471 = OpPhi %float %float_0 %120 %283 %172
               OpSelectionMerge %290 None
               OpBranchConditional %118 %289 %293
        %289 = OpLabel
        %292 = OpFSub %float %float_1 %167
               OpBranch %290
        %293 = OpLabel
               OpBranch %290
        %290 = OpLabel
        %461 = OpPhi %float %292 %289 %167 %293
        %298 = OpCompositeExtract %float %26 0
        %300 = OpExtInst %float %1 Fma %float_2 %298 %float_n1
        %302 = OpCompositeExtract %float %26 1
        %304 = OpExtInst %float %1 Fma %float_n2 %302 %float_1
        %313 = OpCompositeConstruct %v3float %300 %304 %461
        %316 = OpAccessChain %_ptr_Uniform_v4float %20 %int_0 %int_3
        %317 = OpLoad %v4float %316
        %319 = OpAccessChain %_ptr_Uniform_v4float %20 %int_0 %int_2
        %320 = OpLoad %v4float %319
        %323 = OpVectorTimesScalar %v4float %320 %461
        %324 = OpAccessChain %_ptr_Uniform_v4float %20 %int_0 %int_1
        %325 = OpLoad %v4float %324
        %328 = OpVectorTimesScalar %v4float %325 %304
        %329 = OpAccessChain %_ptr_Uniform_v4float %20 %int_0 %int_0
        %330 = OpLoad %v4float %329
        %333 = OpVectorTimesScalar %v4float %330 %300
        %334 = OpFAdd %v4float %328 %333
        %335 = OpFAdd %v4float %323 %334
        %336 = OpFAdd %v4float %317 %335
        %339 = OpVectorShuffle %v2float %336 %336 0 1
        %341 = OpCompositeExtract %float %336 3
        %342 = OpCompositeConstruct %v2float %341 %341
        %343 = OpFDiv %v2float %339 %342
        %346 = OpVectorShuffle %v2float %313 %313 0 1
        %348 = OpFSub %v2float %346 %343
        %350 = OpCompositeExtract %float %348 0
        %351 = OpExtInst %float %1 FAbs %350
        %353 = OpFOrdLessThan %bool %351 %float_4
               OpSelectionMerge %355 None
               OpBranchConditional %353 %354 %355
        %354 = OpLabel
        %357 = OpCompositeExtract %float %348 1
        %358 = OpExtInst %float %1 FAbs %357
        %359 = OpFOrdLessThan %bool %358 %float_4
               OpBranch %355
        %355 = OpLabel
        %360 = OpPhi %bool %353 %290 %359 %354
        %361 = OpLogicalNot %bool %360
        %483 = OpSelect %float %361 %float_1 %471
        %485 = OpCompositeConstruct %v2bool %361 %361
        %486 = OpSelect %v2float %485 %364 %348
        %367 = OpAccessChain %_ptr_Uniform_float %20 %int_16
        %368 = OpLoad %float %367
        %369 = OpFOrdGreaterThan %bool %368 %float_0_5
               OpSelectionMerge %371 None
               OpBranchConditional %369 %370 %371
        %370 = OpLabel
        %374 = OpLoad %54 %373
        %376 = OpSampledImage %62 %374 %61
        %378 = OpImage %54 %376
        %379 = OpImageFetch %v4float %378 %32 Lod %int_0
        %380 = OpVectorShuffle %v3float %379 %379 0 1 2
        %383 = OpLoad %54 %382
        %385 = OpSampledImage %62 %383 %61
        %387 = OpImage %54 %385
        %388 = OpImageFetch %v4float %387 %32 Lod %int_0
        %389 = OpVectorShuffle %v3float %388 %388 0 1 2
        %391 = OpAccessChain %_ptr_Uniform_float %20 %int_17
        %392 = OpLoad %float %391
        %393 = OpFOrdGreaterThan %bool %392 %float_0_5
               OpSelectionMerge %395 None
               OpBranchConditional %393 %394 %395
        %394 = OpLabel
        %397 = OpVectorShuffle %v3float %388 %478 2 1 0
               OpBranch %395
        %395 = OpLabel
        %462 = OpPhi %v3float %389 %370 %397 %394
        %401 = OpFSub %v3float %380 %462
        %402 = OpExtInst %v3float %1 FAbs %401
        %404 = OpCompositeExtract %float %402 0
        %406 = OpCompositeExtract %float %402 1
        %407 = OpExtInst %float %1 FMax %404 %406
        %409 = OpCompositeExtract %float %402 2
        %410 = OpExtInst %float %1 FMax %407 %409
        %412 = OpAccessChain %_ptr_Uniform_float %20 %int_15
        %413 = OpLoad %float %412
        %414 = OpFOrdGreaterThan %bool %410 %413
        %415 = OpSelect %float %414 %float_1 %float_0
               OpBranch %371
        %371 = OpLabel
        %473 = OpPhi %float %float_0 %355 %415 %395
        %421 = OpCompositeExtract %float %486 0
        %422 = OpCompositeExtract %float %486 1
        %423 = OpCompositeConstruct %v4float %421 %422 %483 %473
               OpStore %417 %423
               OpReturn
               OpFunctionEnd
#endif

const uint32_t nfsmw_sgsr2_convertir_ps[] = {
    0x07230203, 0x00010000, 0x000D000A, 0x000001E7, 0x00000000, 0x00020011, 0x00000001, 0x0006000B,
    0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 0x00000000, 0x0003000E, 0x00000000, 0x00000001,
    0x0007000F, 0x00000004, 0x00000004, 0x6E69616D, 0x00000000, 0x0000000C, 0x000001A1, 0x00030010,
    0x00000004, 0x00000007, 0x00040047, 0x0000000C, 0x0000000B, 0x0000000F, 0x00040047, 0x00000011,
    0x00000006, 0x00000010, 0x00050048, 0x00000012, 0x00000000, 0x00000023, 0x00000000, 0x00050048,
    0x00000012, 0x00000001, 0x00000023, 0x00000040, 0x00050048, 0x00000012, 0x00000002, 0x00000023,
    0x00000048, 0x00050048, 0x00000012, 0x00000003, 0x00000023, 0x00000050, 0x00050048, 0x00000012,
    0x00000004, 0x00000023, 0x00000058, 0x00050048, 0x00000012, 0x00000005, 0x00000023, 0x00000060,
    0x00050048, 0x00000012, 0x00000006, 0x00000023, 0x00000068, 0x00050048, 0x00000012, 0x00000007,
    0x00000023, 0x00000070, 0x00050048, 0x00000012, 0x00000008, 0x00000023, 0x00000078, 0x00050048,
    0x00000012, 0x00000009, 0x00000023, 0x00000080, 0x00050048, 0x00000012, 0x0000000A, 0x00000023,
    0x00000088, 0x00050048, 0x00000012, 0x0000000B, 0x00000023, 0x00000090, 0x00050048, 0x00000012,
    0x0000000C, 0x00000023, 0x00000094, 0x00050048, 0x00000012, 0x0000000D, 0x00000023, 0x00000098,
    0x00050048, 0x00000012, 0x0000000E, 0x00000023, 0x0000009C, 0x00050048, 0x00000012, 0x0000000F,
    0x00000023, 0x000000A0, 0x00050048, 0x00000012, 0x00000010, 0x00000023, 0x000000A4, 0x00050048,
    0x00000012, 0x00000011, 0x00000023, 0x000000A8, 0x00050048, 0x00000012, 0x00000012, 0x00000023,
    0x000000AC, 0x00030047, 0x00000012, 0x00000002, 0x00040047, 0x00000014, 0x00000022, 0x00000000,
    0x00040047, 0x00000014, 0x00000021, 0x00000000, 0x00040047, 0x00000038, 0x00000022, 0x00000000,
    0x00040047, 0x00000038, 0x00000021, 0x00000001, 0x00040047, 0x0000003C, 0x00000022, 0x00000000,
    0x00040047, 0x0000003C, 0x00000021, 0x00000004, 0x00040047, 0x00000175, 0x00000022, 0x00000000,
    0x00040047, 0x00000175, 0x00000021, 0x00000002, 0x00040047, 0x0000017E, 0x00000022, 0x00000000,
    0x00040047, 0x0000017E, 0x00000021, 0x00000003, 0x00040047, 0x000001A1, 0x0000001E, 0x00000000,
    0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020,
    0x00040017, 0x00000007, 0x00000006, 0x00000002, 0x00040017, 0x0000000A, 0x00000006, 0x00000004,
    0x00040020, 0x0000000B, 0x00000001, 0x0000000A, 0x0004003B, 0x0000000B, 0x0000000C, 0x00000001,
    0x00040015, 0x0000000F, 0x00000020, 0x00000000, 0x0004002B, 0x0000000F, 0x00000010, 0x00000004,
    0x0004001C, 0x00000011, 0x0000000A, 0x00000010, 0x0015001E, 0x00000012, 0x00000011, 0x00000007,
    0x00000007, 0x00000007, 0x00000007, 0x00000007, 0x00000007, 0x00000007, 0x00000007, 0x00000007,
    0x00000007, 0x00000006, 0x00000006, 0x00000006, 0x00000006, 0x00000006, 0x00000006, 0x00000006,
    0x00000006, 0x00040020, 0x00000013, 0x00000002, 0x00000012, 0x0004003B, 0x00000013, 0x00000014,
    0x00000002, 0x00040015, 0x00000015, 0x00000020, 0x00000001, 0x0004002B, 0x00000015, 0x00000016,
    0x00000003, 0x00040020, 0x00000017, 0x00000002, 0x00000007, 0x00040017, 0x0000001B, 0x00000015,
    0x00000002, 0x0004002B, 0x00000015, 0x00000023, 0x00000007, 0x0004002B, 0x00000015, 0x00000027,
    0x00000008, 0x0004002B, 0x00000006, 0x0000002D, 0x3F000000, 0x0004002B, 0x00000015, 0x0000002F,
    0x00000009, 0x00090019, 0x00000036, 0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000,
    0x00000001, 0x00000000, 0x00040020, 0x00000037, 0x00000000, 0x00000036, 0x0004003B, 0x00000037,
    0x00000038, 0x00000000, 0x0002001A, 0x0000003A, 0x00040020, 0x0000003B, 0x00000000, 0x0000003A,
    0x0004003B, 0x0000003B, 0x0000003C, 0x00000000, 0x0003001B, 0x0000003E, 0x00000036, 0x0004002B,
    0x00000015, 0x00000041, 0x00000000, 0x0004002B, 0x0000000F, 0x00000044, 0x00000000, 0x00040020,
    0x00000045, 0x00000002, 0x00000006, 0x0004002B, 0x00000006, 0x00000048, 0x40000000, 0x0004002B,
    0x00000006, 0x0000004A, 0x00000000, 0x0004002B, 0x0000000F, 0x00000055, 0x00000001, 0x0004002B,
    0x00000015, 0x00000072, 0x0000000E, 0x00020014, 0x00000075, 0x0004002B, 0x00000006, 0x00000079,
    0x3F800000, 0x0007002C, 0x0000000A, 0x0000007A, 0x00000079, 0x00000079, 0x00000079, 0x00000079,
    0x0004002B, 0x00000006, 0x000000AA, 0x3F7FFF58, 0x0004002B, 0x00000006, 0x000000D4, 0x3765D90D,
    0x0004002B, 0x00000015, 0x000000D6, 0x0000000B, 0x0004002B, 0x00000015, 0x000000DA, 0x00000001,
    0x0004002B, 0x00000006, 0x000000EA, 0x33FF8CF1, 0x0004002B, 0x00000006, 0x00000118, 0x3E800000,
    0x00040017, 0x00000132, 0x00000006, 0x00000003, 0x00040020, 0x0000013B, 0x00000002, 0x0000000A,
    0x0004002B, 0x00000015, 0x0000013E, 0x00000002, 0x0004002B, 0x00000006, 0x00000160, 0x40800000,
    0x0005002C, 0x00000007, 0x0000016C, 0x0000004A, 0x0000004A, 0x0004002B, 0x00000015, 0x0000016E,
    0x00000010, 0x0004003B, 0x00000037, 0x00000175, 0x00000000, 0x0004003B, 0x00000037, 0x0000017E,
    0x00000000, 0x0004002B, 0x00000015, 0x00000186, 0x00000011, 0x0004002B, 0x00000015, 0x0000019B,
    0x0000000F, 0x00040020, 0x000001A0, 0x00000003, 0x0000000A, 0x0004003B, 0x000001A0, 0x000001A1,
    0x00000003, 0x0003002E, 0x00000132, 0x000001DE, 0x0004002B, 0x00000006, 0x000001DF, 0xBF000000,
    0x0005002C, 0x00000007, 0x000001E0, 0x000001DF, 0x000001DF, 0x0004002B, 0x00000006, 0x000001E1,
    0xBF800000, 0x0004002B, 0x00000006, 0x000001E2, 0xC0000000, 0x00040017, 0x000001E4, 0x00000075,
    0x00000002, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200F8, 0x00000005,
    0x0004003D, 0x0000000A, 0x0000000D, 0x0000000C, 0x0007004F, 0x00000007, 0x0000000E, 0x0000000D,
    0x0000000D, 0x00000000, 0x00000001, 0x00050041, 0x00000017, 0x00000018, 0x00000014, 0x00000016,
    0x0004003D, 0x00000007, 0x00000019, 0x00000018, 0x00050085, 0x00000007, 0x0000001A, 0x0000000E,
    0x00000019, 0x0004006E, 0x0000001B, 0x00000020, 0x0000000E, 0x00050041, 0x00000017, 0x00000024,
    0x00000014, 0x00000023, 0x0004003D, 0x00000007, 0x00000025, 0x00000024, 0x00050041, 0x00000017,
    0x00000028, 0x00000014, 0x00000027, 0x0004003D, 0x00000007, 0x00000029, 0x00000028, 0x0008000C,
    0x00000007, 0x0000002A, 0x00000001, 0x00000032, 0x0000001A, 0x00000025, 0x00000029, 0x00050041,
    0x00000017, 0x00000030, 0x00000014, 0x0000002F, 0x0004003D, 0x00000007, 0x00000031, 0x00000030,
    0x0008000C, 0x00000007, 0x00000033, 0x00000001, 0x00000032, 0x000001E0, 0x00000031, 0x0000002A,
    0x0004003D, 0x00000036, 0x00000039, 0x00000038, 0x0004003D, 0x0000003A, 0x0000003D, 0x0000003C,
    0x00050056, 0x0000003E, 0x0000003F, 0x00000039, 0x0000003D, 0x00060060, 0x0000000A, 0x00000042,
    0x0000003F, 0x00000033, 0x00000041, 0x00060041, 0x00000045, 0x00000046, 0x00000014, 0x0000002F,
    0x00000044, 0x0004003D, 0x00000006, 0x00000047, 0x00000046, 0x00050085, 0x00000006, 0x00000049,
    0x00000047, 0x00000048, 0x00050050, 0x00000007, 0x0000004B, 0x00000049, 0x0000004A, 0x00050056,
    0x0000003E, 0x0000004F, 0x00000039, 0x0000003D, 0x00050081, 0x00000007, 0x00000052, 0x00000033,
    0x0000004B, 0x00060060, 0x0000000A, 0x00000053, 0x0000004F, 0x00000052, 0x00000041, 0x00060041,
    0x00000045, 0x00000056, 0x00000014, 0x0000002F, 0x00000055, 0x0004003D, 0x00000006, 0x00000057,
    0x00000056, 0x00050085, 0x00000006, 0x00000058, 0x00000057, 0x00000048, 0x00050050, 0x00000007,
    0x00000059, 0x0000004A, 0x00000058, 0x00050056, 0x0000003E, 0x0000005D, 0x00000039, 0x0000003D,
    0x00050081, 0x00000007, 0x00000060, 0x00000033, 0x00000059, 0x00060060, 0x0000000A, 0x00000061,
    0x0000005D, 0x00000060, 0x00000041, 0x00050050, 0x00000007, 0x00000069, 0x00000049, 0x00000058,
    0x00050056, 0x0000003E, 0x0000006D, 0x00000039, 0x0000003D, 0x00050081, 0x00000007, 0x00000070,
    0x00000033, 0x00000069, 0x00060060, 0x0000000A, 0x00000071, 0x0000006D, 0x00000070, 0x00000041,
    0x00050041, 0x00000045, 0x00000073, 0x00000014, 0x00000072, 0x0004003D, 0x00000006, 0x00000074,
    0x00000073, 0x000500BA, 0x00000075, 0x00000076, 0x00000074, 0x0000002D, 0x000300F7, 0x00000078,
    0x00000000, 0x000400FA, 0x00000076, 0x00000077, 0x00000078, 0x000200F8, 0x00000077, 0x00050083,
    0x0000000A, 0x0000007C, 0x0000007A, 0x00000042, 0x00050083, 0x0000000A, 0x0000007E, 0x0000007A,
    0x00000053, 0x00050083, 0x0000000A, 0x00000080, 0x0000007A, 0x00000061, 0x00050083, 0x0000000A,
    0x00000082, 0x0000007A, 0x00000071, 0x000200F9, 0x00000078, 0x000200F8, 0x00000078, 0x000700F5,
    0x0000000A, 0x000001CC, 0x00000071, 0x00000005, 0x00000082, 0x00000077, 0x000700F5, 0x0000000A,
    0x000001CB, 0x00000061, 0x00000005, 0x00000080, 0x00000077, 0x000700F5, 0x0000000A, 0x000001CA,
    0x00000053, 0x00000005, 0x0000007E, 0x00000077, 0x000700F5, 0x0000000A, 0x000001C9, 0x00000042,
    0x00000005, 0x0000007C, 0x00000077, 0x00050051, 0x00000006, 0x00000087, 0x000001C9, 0x00000002,
    0x00050051, 0x00000006, 0x0000008A, 0x000001CA, 0x00000003, 0x0007000C, 0x00000006, 0x0000008B,
    0x00000001, 0x00000025, 0x00000087, 0x0000008A, 0x00050051, 0x00000006, 0x0000008D, 0x000001CB,
    0x00000001, 0x0007000C, 0x00000006, 0x0000008E, 0x00000001, 0x00000025, 0x0000008B, 0x0000008D,
    0x00050051, 0x00000006, 0x00000090, 0x000001CC, 0x00000000, 0x0007000C, 0x00000006, 0x00000091,
    0x00000001, 0x00000025, 0x0000008E, 0x00000090, 0x00050051, 0x00000006, 0x00000094, 0x000001C9,
    0x00000001, 0x00050051, 0x00000006, 0x00000096, 0x000001C9, 0x00000000, 0x0007000C, 0x00000006,
    0x00000097, 0x00000001, 0x00000025, 0x00000094, 0x00000096, 0x0007000C, 0x00000006, 0x0000009A,
    0x00000001, 0x00000025, 0x00000097, 0x00000087, 0x00050051, 0x00000006, 0x0000009C, 0x000001C9,
    0x00000003, 0x0007000C, 0x00000006, 0x0000009D, 0x00000001, 0x00000025, 0x0000009A, 0x0000009C,
    0x00050051, 0x00000006, 0x000000A0, 0x000001CB, 0x00000000, 0x0007000C, 0x00000006, 0x000000A3,
    0x00000001, 0x00000025, 0x00000091, 0x0000009D, 0x00050051, 0x00000006, 0x000000A5, 0x000001CA,
    0x00000000, 0x0007000C, 0x00000006, 0x000000A6, 0x00000001, 0x00000025, 0x000000A3, 0x000000A5,
    0x0007000C, 0x00000006, 0x000000A7, 0x00000001, 0x00000025, 0x000000A0, 0x000000A6, 0x000500B8,
    0x00000075, 0x000000AB, 0x00000091, 0x000000AA, 0x000300F7, 0x000000AD, 0x00000000, 0x000400FA,
    0x000000AB, 0x000000AC, 0x000000AD, 0x000200F8, 0x000000AC, 0x00050051, 0x00000006, 0x000000B0,
    0x000001CA, 0x00000001, 0x0007000C, 0x00000006, 0x000000B3, 0x00000001, 0x00000025, 0x000000B0,
    0x000000A5, 0x00050051, 0x00000006, 0x000000B5, 0x000001CA, 0x00000002, 0x0007000C, 0x00000006,
    0x000000B6, 0x00000001, 0x00000025, 0x000000B3, 0x000000B5, 0x0007000C, 0x00000006, 0x000000B9,
    0x00000001, 0x00000025, 0x000000B6, 0x0000008A, 0x0007000C, 0x00000006, 0x000000BF, 0x00000001,
    0x00000025, 0x0000008D, 0x000000A0, 0x00050051, 0x00000006, 0x000000C1, 0x000001CB, 0x00000002,
    0x0007000C, 0x00000006, 0x000000C2, 0x00000001, 0x00000025, 0x000000BF, 0x000000C1, 0x00050051,
    0x00000006, 0x000000C4, 0x000001CB, 0x00000003, 0x0007000C, 0x00000006, 0x000000C5, 0x00000001,
    0x00000025, 0x000000C2, 0x000000C4, 0x00050051, 0x00000006, 0x000000C8, 0x000001CC, 0x00000001,
    0x0007000C, 0x00000006, 0x000000CB, 0x00000001, 0x00000025, 0x000000C8, 0x00000090, 0x00050051,
    0x00000006, 0x000000CD, 0x000001CC, 0x00000002, 0x0007000C, 0x00000006, 0x000000CE, 0x00000001,
    0x00000025, 0x000000CB, 0x000000CD, 0x00050051, 0x00000006, 0x000000D0, 0x000001CC, 0x00000003,
    0x0007000C, 0x00000006, 0x000000D1, 0x00000001, 0x00000025, 0x000000CE, 0x000000D0, 0x00050041,
    0x00000045, 0x000000D7, 0x00000014, 0x000000D6, 0x0004003D, 0x00000006, 0x000000D8, 0x000000D7,
    0x00050041, 0x00000017, 0x000000DB, 0x00000014, 0x000000DA, 0x0004003D, 0x00000007, 0x000000DC,
    0x000000DB, 0x0006000C, 0x00000006, 0x000000DD, 0x00000001, 0x00000042, 0x000000DC, 0x00050085,
    0x00000006, 0x000000E1, 0x000000D4, 0x000000D8, 0x00050085, 0x00000006, 0x000000E3, 0x000000E1,
    0x000000DD, 0x00050083, 0x00000006, 0x000000E7, 0x00000079, 0x00000091, 0x00050085, 0x00000006,
    0x000000E8, 0x000000E3, 0x000000E7, 0x00050083, 0x00000006, 0x000000EE, 0x00000091, 0x0000009D,
    0x0006000C, 0x00000006, 0x000000EF, 0x00000001, 0x00000004, 0x000000EE, 0x00050081, 0x00000006,
    0x000000F1, 0x000000EF, 0x000000EA, 0x00050088, 0x00000006, 0x000000F2, 0x000000E8, 0x000000F1,
    0x0008000C, 0x00000006, 0x000000F3, 0x00000001, 0x0000002B, 0x000000F2, 0x0000004A, 0x00000079,
    0x00050083, 0x00000006, 0x000000F9, 0x00000091, 0x000000B9, 0x0006000C, 0x00000006, 0x000000FA,
    0x00000001, 0x00000004, 0x000000F9, 0x00050081, 0x00000006, 0x000000FC, 0x000000FA, 0x000000EA,
    0x00050088, 0x00000006, 0x000000FD, 0x000000E8, 0x000000FC, 0x0008000C, 0x00000006, 0x000000FE,
    0x00000001, 0x0000002B, 0x000000FD, 0x0000004A, 0x00000079, 0x00050081, 0x00000006, 0x00000100,
    0x000000F3, 0x000000FE, 0x00050083, 0x00000006, 0x00000104, 0x00000091, 0x000000C5, 0x0006000C,
    0x00000006, 0x00000105, 0x00000001, 0x00000004, 0x00000104, 0x00050081, 0x00000006, 0x00000107,
    0x00000105, 0x000000EA, 0x00050088, 0x00000006, 0x00000108, 0x000000E8, 0x00000107, 0x0008000C,
    0x00000006, 0x00000109, 0x00000001, 0x0000002B, 0x00000108, 0x0000004A, 0x00000079, 0x00050081,
    0x00000006, 0x0000010B, 0x00000100, 0x00000109, 0x00050083, 0x00000006, 0x0000010F, 0x00000091,
    0x000000D1, 0x0006000C, 0x00000006, 0x00000110, 0x00000001, 0x00000004, 0x0000010F, 0x00050081,
    0x00000006, 0x00000112, 0x00000110, 0x000000EA, 0x00050088, 0x00000006, 0x00000113, 0x000000E8,
    0x00000112, 0x0008000C, 0x00000006, 0x00000114, 0x00000001, 0x0000002B, 0x00000113, 0x0000004A,
    0x00000079, 0x00050081, 0x00000006, 0x00000116, 0x0000010B, 0x00000114, 0x0004007F, 0x00000006,
    0x000001DB, 0x00000116, 0x0008000C, 0x00000006, 0x0000011A, 0x00000001, 0x00000032, 0x000001DB,
    0x00000118, 0x00000079, 0x0008000C, 0x00000006, 0x0000011B, 0x00000001, 0x0000002B, 0x0000011A,
    0x0000004A, 0x00000079, 0x000200F9, 0x000000AD, 0x000200F8, 0x000000AD, 0x000700F5, 0x00000006,
    0x000001D7, 0x0000004A, 0x00000078, 0x0000011B, 0x000000AC, 0x000300F7, 0x00000122, 0x00000000,
    0x000400FA, 0x00000076, 0x00000121, 0x00000125, 0x000200F8, 0x00000121, 0x00050083, 0x00000006,
    0x00000124, 0x00000079, 0x000000A7, 0x000200F9, 0x00000122, 0x000200F8, 0x00000125, 0x000200F9,
    0x00000122, 0x000200F8, 0x00000122, 0x000700F5, 0x00000006, 0x000001CD, 0x00000124, 0x00000121,
    0x000000A7, 0x00000125, 0x00050051, 0x00000006, 0x0000012A, 0x0000001A, 0x00000000, 0x0008000C,
    0x00000006, 0x0000012C, 0x00000001, 0x00000032, 0x00000048, 0x0000012A, 0x000001E1, 0x00050051,
    0x00000006, 0x0000012E, 0x0000001A, 0x00000001, 0x0008000C, 0x00000006, 0x00000130, 0x00000001,
    0x00000032, 0x000001E2, 0x0000012E, 0x00000079, 0x00060050, 0x00000132, 0x00000139, 0x0000012C,
    0x00000130, 0x000001CD, 0x00060041, 0x0000013B, 0x0000013C, 0x00000014, 0x00000041, 0x00000016,
    0x0004003D, 0x0000000A, 0x0000013D, 0x0000013C, 0x00060041, 0x0000013B, 0x0000013F, 0x00000014,
    0x00000041, 0x0000013E, 0x0004003D, 0x0000000A, 0x00000140, 0x0000013F, 0x0005008E, 0x0000000A,
    0x00000143, 0x00000140, 0x000001CD, 0x00060041, 0x0000013B, 0x00000144, 0x00000014, 0x00000041,
    0x000000DA, 0x0004003D, 0x0000000A, 0x00000145, 0x00000144, 0x0005008E, 0x0000000A, 0x00000148,
    0x00000145, 0x00000130, 0x00060041, 0x0000013B, 0x00000149, 0x00000014, 0x00000041, 0x00000041,
    0x0004003D, 0x0000000A, 0x0000014A, 0x00000149, 0x0005008E, 0x0000000A, 0x0000014D, 0x0000014A,
    0x0000012C, 0x00050081, 0x0000000A, 0x0000014E, 0x00000148, 0x0000014D, 0x00050081, 0x0000000A,
    0x0000014F, 0x00000143, 0x0000014E, 0x00050081, 0x0000000A, 0x00000150, 0x0000013D, 0x0000014F,
    0x0007004F, 0x00000007, 0x00000153, 0x00000150, 0x00000150, 0x00000000, 0x00000001, 0x00050051,
    0x00000006, 0x00000155, 0x00000150, 0x00000003, 0x00050050, 0x00000007, 0x00000156, 0x00000155,
    0x00000155, 0x00050088, 0x00000007, 0x00000157, 0x00000153, 0x00000156, 0x0007004F, 0x00000007,
    0x0000015A, 0x00000139, 0x00000139, 0x00000000, 0x00000001, 0x00050083, 0x00000007, 0x0000015C,
    0x0000015A, 0x00000157, 0x00050051, 0x00000006, 0x0000015E, 0x0000015C, 0x00000000, 0x0006000C,
    0x00000006, 0x0000015F, 0x00000001, 0x00000004, 0x0000015E, 0x000500B8, 0x00000075, 0x00000161,
    0x0000015F, 0x00000160, 0x000300F7, 0x00000163, 0x00000000, 0x000400FA, 0x00000161, 0x00000162,
    0x00000163, 0x000200F8, 0x00000162, 0x00050051, 0x00000006, 0x00000165, 0x0000015C, 0x00000001,
    0x0006000C, 0x00000006, 0x00000166, 0x00000001, 0x00000004, 0x00000165, 0x000500B8, 0x00000075,
    0x00000167, 0x00000166, 0x00000160, 0x000200F9, 0x00000163, 0x000200F8, 0x00000163, 0x000700F5,
    0x00000075, 0x00000168, 0x00000161, 0x00000122, 0x00000167, 0x00000162, 0x000400A8, 0x00000075,
    0x00000169, 0x00000168, 0x000600A9, 0x00000006, 0x000001E3, 0x00000169, 0x00000079, 0x000001D7,
    0x00050050, 0x000001E4, 0x000001E5, 0x00000169, 0x00000169, 0x000600A9, 0x00000007, 0x000001E6,
    0x000001E5, 0x0000016C, 0x0000015C, 0x00050041, 0x00000045, 0x0000016F, 0x00000014, 0x0000016E,
    0x0004003D, 0x00000006, 0x00000170, 0x0000016F, 0x000500BA, 0x00000075, 0x00000171, 0x00000170,
    0x0000002D, 0x000300F7, 0x00000173, 0x00000000, 0x000400FA, 0x00000171, 0x00000172, 0x00000173,
    0x000200F8, 0x00000172, 0x0004003D, 0x00000036, 0x00000176, 0x00000175, 0x00050056, 0x0000003E,
    0x00000178, 0x00000176, 0x0000003D, 0x00040064, 0x00000036, 0x0000017A, 0x00000178, 0x0007005F,
    0x0000000A, 0x0000017B, 0x0000017A, 0x00000020, 0x00000002, 0x00000041, 0x0008004F, 0x00000132,
    0x0000017C, 0x0000017B, 0x0000017B, 0x00000000, 0x00000001, 0x00000002, 0x0004003D, 0x00000036,
    0x0000017F, 0x0000017E, 0x00050056, 0x0000003E, 0x00000181, 0x0000017F, 0x0000003D, 0x00040064,
    0x00000036, 0x00000183, 0x00000181, 0x0007005F, 0x0000000A, 0x00000184, 0x00000183, 0x00000020,
    0x00000002, 0x00000041, 0x0008004F, 0x00000132, 0x00000185, 0x00000184, 0x00000184, 0x00000000,
    0x00000001, 0x00000002, 0x00050041, 0x00000045, 0x00000187, 0x00000014, 0x00000186, 0x0004003D,
    0x00000006, 0x00000188, 0x00000187, 0x000500BA, 0x00000075, 0x00000189, 0x00000188, 0x0000002D,
    0x000300F7, 0x0000018B, 0x00000000, 0x000400FA, 0x00000189, 0x0000018A, 0x0000018B, 0x000200F8,
    0x0000018A, 0x0008004F, 0x00000132, 0x0000018D, 0x00000184, 0x000001DE, 0x00000002, 0x00000001,
    0x00000000, 0x000200F9, 0x0000018B, 0x000200F8, 0x0000018B, 0x000700F5, 0x00000132, 0x000001CE,
    0x00000185, 0x00000172, 0x0000018D, 0x0000018A, 0x00050083, 0x00000132, 0x00000191, 0x0000017C,
    0x000001CE, 0x0006000C, 0x00000132, 0x00000192, 0x00000001, 0x00000004, 0x00000191, 0x00050051,
    0x00000006, 0x00000194, 0x00000192, 0x00000000, 0x00050051, 0x00000006, 0x00000196, 0x00000192,
    0x00000001, 0x0007000C, 0x00000006, 0x00000197, 0x00000001, 0x00000028, 0x00000194, 0x00000196,
    0x00050051, 0x00000006, 0x00000199, 0x00000192, 0x00000002, 0x0007000C, 0x00000006, 0x0000019A,
    0x00000001, 0x00000028, 0x00000197, 0x00000199, 0x00050041, 0x00000045, 0x0000019C, 0x00000014,
    0x0000019B, 0x0004003D, 0x00000006, 0x0000019D, 0x0000019C, 0x000500BA, 0x00000075, 0x0000019E,
    0x0000019A, 0x0000019D, 0x000600A9, 0x00000006, 0x0000019F, 0x0000019E, 0x00000079, 0x0000004A,
    0x000200F9, 0x00000173, 0x000200F8, 0x00000173, 0x000700F5, 0x00000006, 0x000001D9, 0x0000004A,
    0x00000163, 0x0000019F, 0x0000018B, 0x00050051, 0x00000006, 0x000001A5, 0x000001E6, 0x00000000,
    0x00050051, 0x00000006, 0x000001A6, 0x000001E6, 0x00000001, 0x00070050, 0x0000000A, 0x000001A7,
    0x000001A5, 0x000001A6, 0x000001E3, 0x000001D9, 0x0003003E, 0x000001A1, 0x000001A7, 0x000100FD,
    0x00010038,
};

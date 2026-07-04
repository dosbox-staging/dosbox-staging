// Library-API spike: glslang C++ compile + SPIRV-Cross reflection,
// verifying UniformPacker's canonical std140 offsets against reality.
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <spirv_cross/spirv_cross.hpp>

static const char* FragSource = R"GLSL(
#version 450

layout(set = 0, binding = 0, std140) uniform Uniforms {
	vec2 OUTPUT_SIZE;
	vec2 INPUT_SIZE_0;
	float PARAM_A;
	float PARAM_B;
	vec2 PARAM_C;
};

#if defined(FRAGMENT)

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D INPUT_TEXTURE_0;

void main() {
	FragColor = texture(INPUT_TEXTURE_0, v_texCoord / INPUT_SIZE_0);
}

#endif
)GLSL";

int main()
{
	glslang::InitializeProcess();

	glslang::TShader shader(EShLangFragment);
	const char* sources[] = {FragSource};
	shader.setStrings(sources, 1);
	shader.setPreamble("#define FRAGMENT 1\n");
	shader.setEnvInput(glslang::EShSourceGlsl, EShLangFragment,
	                   glslang::EShClientVulkan, 450);
	shader.setEnvClient(glslang::EShClientVulkan,
	                    glslang::EShTargetVulkan_1_3);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

	if (!shader.parse(GetDefaultResources(), 450, false, EShMsgDefault)) {
		printf("PARSE FAILED:\n%s\n", shader.getInfoLog());
		return 1;
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(EShMsgDefault)) {
		printf("LINK FAILED:\n%s\n", program.getInfoLog());
		return 1;
	}

	std::vector<uint32_t> spirv;
	glslang::GlslangToSpv(*program.getIntermediate(EShLangFragment), spirv);
	printf("glslang OK: %zu SPIR-V words\n", spirv.size());

	glslang::FinalizeProcess();

	// --- reflection ---
	spirv_cross::Compiler comp(spirv);
	auto resources = comp.get_shader_resources();
	printf("uniform buffers: %zu, sampled images: %zu\n",
	       resources.uniform_buffers.size(),
	       resources.sampled_images.size());

	assert(resources.uniform_buffers.size() == 1);
	const auto& ubo  = resources.uniform_buffers[0];
	const auto& type = comp.get_type(ubo.base_type_id);

	printf("UBO '%s' (set=%u binding=%u), declared size=%zu\n",
	       comp.get_name(ubo.id).c_str(),
	       comp.get_decoration(ubo.id, spv::DecorationDescriptorSet),
	       comp.get_decoration(ubo.id, spv::DecorationBinding),
	       comp.get_declared_struct_size(type));

	// UniformPacker's canonical std140 expectations:
	const uint32_t expected[] = {0, 8, 16, 20, 24};
	const char* names[]       = {"OUTPUT_SIZE", "INPUT_SIZE_0", "PARAM_A",
	                             "PARAM_B", "PARAM_C"};

	bool all_ok = true;
	for (uint32_t i = 0; i < type.member_types.size(); ++i) {
		const auto offset = comp.type_struct_member_offset(type, i);
		const auto& name  = comp.get_member_name(type.self, i);
		const bool ok = (offset == expected[i]) && (name == names[i]);
		printf("  member %u: %-14s offset=%2u  expected=%2u  %s\n",
		       i, name.c_str(), offset, expected[i],
		       ok ? "OK" : "MISMATCH");
		all_ok &= ok;
	}

	const auto& img = resources.sampled_images[0];
	printf("sampler '%s' binding=%u\n",
	       comp.get_name(img.id).c_str(),
	       comp.get_decoration(img.id, spv::DecorationBinding));

	printf(all_ok ? "\nSPIKE PASSED: packer math == reflection\n"
	              : "\nSPIKE FAILED\n");
	return all_ok ? 0 : 1;
}

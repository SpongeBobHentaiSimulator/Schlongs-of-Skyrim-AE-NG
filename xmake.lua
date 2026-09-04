set_project("SchlongsOfSkyrim")
set_version("3.0.0")

set_languages("c++23")
set_arch("x64")

-- Dependencias
add_requires("nlohmann_json", "simpleini")

includes("extern/CommonLibSSE-NG")

target("SchlongsOfSkyrim")
	set_kind("shared")
	
	-- Prevenir macros min/max de Windows
	add_defines("NOMINMAX")

	set_pcxxheader("src/PCH.h")
	
	add_packages("nlohmann_json", "simpleini")
	add_deps("commonlibsse-ng")
	add_rules("commonlibsse-ng.plugin", {
		name = "SchlongsOfSkyrim",
		author = "SpongeBobHentaiSimulator",
		description = "New Version of SOS Made for AE"
	})

	add_files("src/**.cpp")
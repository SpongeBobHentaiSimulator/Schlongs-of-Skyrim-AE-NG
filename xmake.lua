set_project("SchlongsOfSkyrim")
set_version("4.0.0")

set_languages("c++23")
set_arch("x64")

add_requires("nlohmann_json", "simpleini")

includes("extern/CommonLibSSE-NG")

target("SchlongsOfSkyrim")
	set_kind("shared")

	add_defines("NOMINMAX", "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")

	set_pcxxheader("src/PCH.h")

	add_packages("nlohmann_json", "simpleini")
	add_deps("commonlibsse-ng")
	add_rules("commonlibsse-ng.plugin", {
		name = "SchlongsOfSkyrim",
		author = "SpongeBobHentaiSimulator",
		description = "New Version of SOS Made for AE"
	})

	add_files("src/**.cpp")

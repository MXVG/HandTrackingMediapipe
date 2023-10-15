load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")


new_git_repository(
    name = "glfw",
    remote = "https://github.com/glfw/glfw.git",
    commit = "1ad5df803239250c43e021f92fe87b738f231f8b",
    build_file = "@//:glfw.BUILD",
)

new_git_repository(
    name = "glm",
    remote = "https://github.com/g-truc/glm.git",
    commit = "658d8960d081e0c9c312d49758c7ef919371b428",
    build_file = "@//:glm.BUILD",
)

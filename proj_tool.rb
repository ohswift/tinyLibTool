#encoding: utf-8

require 'xcodeproj'
require 'cocoapods'

project_path = './output/libSim.xcodeproj'
project = Xcodeproj::Project.open(project_path)

def removeBuildPhaseFilesRecursively(aTarget, aGroup)
  aGroup.files.each do |file|
    if file.real_path.to_s.end_with?(".m", ".mm", ".cpp") then
      aTarget.source_build_phase.remove_file_reference(file)
    elsif file.real_path.to_s.end_with?(".plist") then
      aTarget.resources_build_phase.remove_file_reference(file)
    end
  end

  aGroup.groups.each do |group|
    removeBuildPhaseFilesRecursively(aTarget, group)
  end
end

def addFilesToGroup(project, aTarget, aGroup)
  Dir.foreach(aGroup.real_path) do |entry|
    filePath = File.join(aGroup.real_path, entry)
    p "filePath: #{filePath}"
    if !File.directory?(filePath) && entry != ".DS_Store" then
          fileReference = aGroup.new_reference(filePath)
      if filePath.to_s.end_with?("pbobjc.m", "pbobjc.mm") then
        aTarget.add_file_references([fileReference], '-fno-objc-arc')
      elsif filePath.to_s.end_with?(".m", ".mm", ".cpp") then
        aTarget.source_build_phase.add_file_reference(fileReference, true)
      elsif filePath.to_s.end_with?(".plist") then
        aTarget.resources_build_phase.add_file_reference(fileReference, true)
      end
    elsif File.directory?(filePath) && entry != '.' && entry != '..' then
      hierarchy_path = aGroup.hierarchy_path[1, aGroup.hierarchy_path.length]
      subGroup = project.main_group.find_subpath(hierarchy_path + '/' + entry, true)
      subGroup.set_source_tree(aGroup.source_tree)
      subGroup.set_path(aGroup.real_path + entry)
      addFilesToGroup(project, aTarget, subGroup)
    end
  end
end


project.targets.each do |target|
  puts target.name
end

p project_path

target = project.targets.first
srcGroup = project.main_group.find_subpath('src', true)
srcGroup.set_source_tree('<group>')
srcGroup.set_path('src')

if !srcGroup.empty? then
  removeBuildPhaseFilesRecursively(target, srcGroup)
  srcGroup.clear()
end

addFilesToGroup(project, target, srcGroup)
project.save

system "open #{project_path}"

# target.source_build_phase.files.each do |nfile|
#   puts nfile
# end

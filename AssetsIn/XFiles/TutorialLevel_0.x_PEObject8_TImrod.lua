
-- this script is in lua format
-- this is a meta script that fills in data that is passed to 'myScript' that in turn calls C++ function
-- some of the data can be set by default, some of the data might be required to be edited from maya
function fillMetaInfoTable(args) -- the script fromat requires existance of this function

-- user modifed data

-- TODO: use ImrodEnemy.lua when it is implemented
args['myScript']="StaticMesh.lua"
args['myScriptPackage']="Default"

-- needed for StaticMesh
args['meshName'] = "imrod.x_main_mesh.mesha"
args['meshPackage'] = "ExportTutorial"

end -- required

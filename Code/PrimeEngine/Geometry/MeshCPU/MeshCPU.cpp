// APIAbstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Immediate header
#include "MeshCPU.h"

// Outer-Engine includes

// Inter-Engine includes
#include "PrimeEngine/APIAbstraction/Effect/EffectManager.h"
#include "../MaterialCPU/MaterialSetCPU.h"
#include "../TangentBufferCPU/TangentBufferCPU.h"
#include "../PositionBufferCPU/PositionBufferCPUManager.h"
#include "../NormalBufferCPU/NormalBufferCPUManager.h"
#include "../TexCoordBufferCPU/TexCoordBufferCPUManager.h"
// Sibling/Children includes

namespace PE {

// Reads the specified buffer from file
void MeshCPU::ReadMesh(const char *filename, const char *package, const char *tag)
{
    PEString::generatePathname(*m_pContext, filename, package, "Meshes", PEString::s_buf, PEString::BUF_SIZE);
	
	// Path is now a full path to the file with the filename itself
	FileReader f(PEString::s_buf);

	char line[256];
	f.nextNonEmptyLine(line, 255);
	// TODO : make sure it is "MESH"

	// Vertex buffer filename
	char vbfilename[256];
	f.nextNonEmptyLine(vbfilename, 255);
	StringOps::strcmp(vbfilename, "none");
	
	m_hPositionBufferCPU = PositionBufferCPUManager::Instance()->ReadVertexBuffer(vbfilename, package, tag);
	
	// Index buffer filename
	char ibfilename[256];
	f.nextNonEmptyLine(ibfilename, 255);
	assert(StringOps::strcmp(ibfilename, "none"));

	m_hIndexBufferCPU = PositionBufferCPUManager::Instance()->ReadIndexBuffer(ibfilename, package, tag);
	
	
	// TexCoord buffer filename
	char tcfilename[256];
	f.nextNonEmptyLine(tcfilename, 255);

	if (StringOps::strcmp(tcfilename, "none") != 0)
	{
		m_hTexCoordBufferCPU = PositionBufferCPUManager::Instance()->ReadTexCoordBuffer(tcfilename, package, tag);
		
	}
	else
	{
		// create mock
		m_hTexCoordBufferCPU = Handle("TEXCOORD_BUFFER_CPU", sizeof(TexCoordBufferCPU));
		TexCoordBufferCPU *ptcb = new(m_hTexCoordBufferCPU) TexCoordBufferCPU(*m_pContext, m_arena);
		ptcb->createMockCPUBuffer(m_hPositionBufferCPU.getObject<PositionBufferCPU>()->m_values.m_size / 3);
	}

	// Normal buffer filename
	char nbfilename[256];
	f.nextNonEmptyLine(nbfilename, 255);
	if(StringOps::strcmp(nbfilename, "none") != 0)
	{
		m_hNormalBufferCPU = NormalBufferCPUManager::Instance()->ReadNormalBuffer(nbfilename, package, tag);
	}

	// Tangent buffer filename
	char tbfilename[256];
	f.nextNonEmptyLine(tbfilename, 255);
	if(StringOps::strcmp(tbfilename, "none") != 0)
	{
		m_hTangentBufferCPU = PositionBufferCPUManager::Instance()->ReadTangentBuffer(tbfilename, package, tag);
	}
	/*
	this was enabled at some point. not sure why it was forcing having tangent buffer..
	else
	{
		m_hTangentBufferCPU = Handle(TANGENT_BUFFER_CPU, sizeof(TangentBufferCPU));
		TangentBufferCPU *ptb = new(m_hTangentBufferCPU) TangentBufferCPU();
		ptb->createMockCPUBuffer(m_hPositionBufferCPU.getObject<PositionBufferCPU>()->m_values.m_size / 3);
	}*/

	// MaterialSet filename
	char msfilename[256];
	f.nextNonEmptyLine(msfilename, 255);

	m_hMaterialSetCPU = PositionBufferCPUManager::Instance()->ReadMaterialSetCPU(msfilename, package);

	// Skin weights
	char swfilename[256];
	f.nextNonEmptyLine(swfilename, 255);
	if (StringOps::strcmp(swfilename, "none") != 0)
	{
		m_hSkinWeightsCPU = PositionBufferCPUManager::Instance()->ReadSkinWeights(swfilename, package, tag);
	}

	char optionalLine[256];
	while (f.nextNonEmptyLine(optionalLine, 255))
	{
		// have another line.. assume is additional meshes for blend shapes
		if (m_hAdditionalVertexBuffersCPU.m_capacity == 0)
		{
			m_hAdditionalVertexBuffersCPU.reset(16);
		}
		Handle additionalPositionBufferCPU = PositionBufferCPUManager::Instance()->ReadVertexBuffer(optionalLine, package, tag);
		m_hAdditionalVertexBuffersCPU.add(additionalPositionBufferCPU);
		
		f.nextNonEmptyLine(optionalLine, 255);
		if (m_hAdditionalTexCoordBuffersCPU.m_capacity == 0)
		{
			m_hAdditionalTexCoordBuffersCPU.reset(16);
		}
		
		Handle hAdditionalTexCoordBufferCPU = TexCoordBufferCPUManager::Instance()->ReadTexCoordBuffer(optionalLine, package, tag);
		m_hAdditionalTexCoordBuffersCPU.add(hAdditionalTexCoordBufferCPU);
	
		f.nextNonEmptyLine(optionalLine, 255);
		if (m_hAdditionalNormalBuffersCPU.m_capacity == 0)
		{
			m_hAdditionalNormalBuffersCPU.reset(16);
		}
		Handle hAdditionalNormalBufferCPU = NormalBufferCPUManager::Instance()->ReadNormalBuffer(optionalLine, package, tag);
		m_hAdditionalNormalBuffersCPU.add(hAdditionalNormalBufferCPU);
	}

	

}


void MeshCPU::createEmptyMesh()
{
	m_hPositionBufferCPU = Handle("VERTEX_BUFFER_CPU", sizeof(PositionBufferCPU));
	PositionBufferCPU *pvb = new(m_hPositionBufferCPU) PositionBufferCPU(*m_pContext, m_arena);
	pvb->createEmptyCPUBuffer();

	m_hIndexBufferCPU = Handle("INDEX_BUFFER_CPU", sizeof(IndexBufferCPU));
	IndexBufferCPU *pib = new(m_hIndexBufferCPU) IndexBufferCPU(*m_pContext, m_arena);
	pib->createEmptyCPUBuffer();

	m_hColorBufferCPU = Handle("COLOR_BUFFER_CPU", sizeof(ColorBufferCPU));
	ColorBufferCPU *pcb = new(m_hColorBufferCPU) ColorBufferCPU(*m_pContext, m_arena);
	pcb->createEmptyCPUBuffer();

	//m_hTexCoordBufferCPU = Handle(TEXCOORD_BUFFER_CPU, sizeof(TexCoordBufferCPU));
	//TexCoordBufferCPU *ptcb = new(m_hTexCoordBufferCPU) TexCoordBufferCPU();
	//ptcb->createBillboardCPUBuffer();

	//m_hNormalBufferCPU = Handle("NORMAL_BUFFER_CPU", sizeof(NormalBufferCPU));
	//NormalBufferCPU *pnb = new(m_hNormalBufferCPU) NormalBufferCPU();
	//pnb->createBillboardCPUBuffer();

	//m_hTangentBufferCPU

	m_hMaterialSetCPU = Handle("MATERIAL_SET_CPU", sizeof(MaterialSetCPU));
	MaterialSetCPU *pmscpu = new(m_hMaterialSetCPU) MaterialSetCPU(*m_pContext, m_arena);
	pmscpu->createSetWithOneDefaultMaterial();
}

void MeshCPU::createBillboardMesh()
{
	m_hPositionBufferCPU = Handle("VERTEX_BUFFER_CPU", sizeof(PositionBufferCPU));
	PositionBufferCPU *pvb = new(m_hPositionBufferCPU) PositionBufferCPU(*m_pContext, m_arena);
	pvb->createBillboardCPUBuffer(100.0f, 100.0f);
	
	m_hIndexBufferCPU = Handle("INDEX_BUFFER_CPU", sizeof(IndexBufferCPU));
	IndexBufferCPU *pib = new(m_hIndexBufferCPU) IndexBufferCPU(*m_pContext, m_arena);
	pib->createBillboardCPUBuffer();
	
	//m_hTexCoordBufferCPU = Handle(TEXCOORD_BUFFER_CPU, sizeof(TexCoordBufferCPU));
	//TexCoordBufferCPU *ptcb = new(m_hTexCoordBufferCPU) TexCoordBufferCPU();
	//ptcb->createBillboardCPUBuffer();

	m_hNormalBufferCPU = Handle("NORMAL_BUFFER_CPU", sizeof(NormalBufferCPU));
	NormalBufferCPU *pnb = new(m_hNormalBufferCPU) NormalBufferCPU(*m_pContext, m_arena);
	pnb->createBillboardCPUBuffer();
	
	//m_hTangentBufferCPU
	
	m_hMaterialSetCPU = Handle("MATERIAL_SET_CPU", sizeof(MaterialSetCPU));
	MaterialSetCPU *pmscpu = new(m_hMaterialSetCPU) MaterialSetCPU(*m_pContext, m_arena);
	pmscpu->createSetWithOneDefaultMaterial();
}

void MeshCPU::createBillboardMeshWithColorTexture(const char *textureFilename, const char *package, PrimitiveTypes::Float32 w, PrimitiveTypes::Float32 h, ESamplerState customSamplerState/* = SamplerState_Count*/)
{
	m_hPositionBufferCPU = Handle("VERTEX_BUFFER_CPU", sizeof(PositionBufferCPU));
	PositionBufferCPU *pvb = new(m_hPositionBufferCPU) PositionBufferCPU(*m_pContext, m_arena);
	pvb->createBillboardCPUBuffer(w, h);
	
	m_hIndexBufferCPU = Handle("INDEX_BUFFER_CPU", sizeof(IndexBufferCPU));
	IndexBufferCPU *pib = new(m_hIndexBufferCPU) IndexBufferCPU(*m_pContext, m_arena);
	pib->createBillboardCPUBuffer();
	
	m_hTexCoordBufferCPU = Handle("TEXCOORD_BUFFER_CPU", sizeof(TexCoordBufferCPU));
	TexCoordBufferCPU *ptcb = new(m_hTexCoordBufferCPU) TexCoordBufferCPU(*m_pContext, m_arena);
	ptcb->createBillboardCPUBuffer();

	m_hNormalBufferCPU = Handle("NORMAL_BUFFER_CPU", sizeof(NormalBufferCPU));
	NormalBufferCPU *pnb = new(m_hNormalBufferCPU) NormalBufferCPU(*m_pContext, m_arena);
	pnb->createBillboardCPUBuffer();
	
	//m_hTangentBufferCPU
	
	m_hMaterialSetCPU = Handle("MATERIAL_SET_CPU", sizeof(MaterialSetCPU));
	MaterialSetCPU *pmscpu = new(m_hMaterialSetCPU) MaterialSetCPU(*m_pContext, m_arena);
	pmscpu->createSetWithOneTexturedMaterial(textureFilename, package, customSamplerState);
}

void MeshCPU::createBillboardMeshWithColorGlowTextures(const char *colorTextureFilename, const char *glowTextureFilename, const char *package,
	PrimitiveTypes::Float32 w, PrimitiveTypes::Float32 h)
{
	m_hPositionBufferCPU = Handle("VERTEX_BUFFER_CPU", sizeof(PositionBufferCPU));
	PositionBufferCPU *pvb = new(m_hPositionBufferCPU) PositionBufferCPU(*m_pContext, m_arena);
	pvb->createBillboardCPUBuffer(w, h);
	
	m_hIndexBufferCPU = Handle("INDEX_BUFFER_CPU", sizeof(IndexBufferCPU));
	IndexBufferCPU *pib = new(m_hIndexBufferCPU) IndexBufferCPU(*m_pContext, m_arena);
	pib->createBillboardCPUBuffer();
	
	m_hTexCoordBufferCPU = Handle("TEXCOORD_BUFFER_CPU", sizeof(TexCoordBufferCPU));
	TexCoordBufferCPU *ptcb = new(m_hTexCoordBufferCPU) TexCoordBufferCPU(*m_pContext, m_arena);
	ptcb->createBillboardCPUBuffer();

	m_hNormalBufferCPU = Handle("NORMAL_BUFFER_CPU", sizeof(NormalBufferCPU));
	NormalBufferCPU *pnb = new(m_hNormalBufferCPU) NormalBufferCPU(*m_pContext, m_arena);
	pnb->createBillboardCPUBuffer();
	
	//m_hTangentBufferCPU
	
	m_hMaterialSetCPU = Handle("MATERIAL_SET_CPU", sizeof(MaterialSetCPU));
	MaterialSetCPU *pmscpu = new(m_hMaterialSetCPU) MaterialSetCPU(*m_pContext, m_arena);
	pmscpu->createSetWithOneTexturedMaterialWithGlow(colorTextureFilename, glowTextureFilename, package);
}

// ================ AABB building utility functions =========================
static bool isFinite3(const Vector3& v) {
	using std::isfinite;
	return isfinite(v.m_x) && isfinite(v.m_y) && isfinite(v.m_z);
}
static Vector3 compMin(const Vector3& a, const Vector3& b) {
	return Vector3(
		a.m_x < b.m_x ? a.m_x : b.m_x,
		a.m_y < b.m_y ? a.m_y : b.m_y,
		a.m_z < b.m_z ? a.m_z : b.m_z
	);
}

static Vector3 compMax(const Vector3& a, const Vector3& b) {
	return Vector3(
		a.m_x > b.m_x ? a.m_x : b.m_x,
		a.m_y > b.m_y ? a.m_y : b.m_y,
		a.m_z > b.m_z ? a.m_z : b.m_z
	);
}
// ================ AABB building utility functions =========================

void MeshCPU::buildLocalAABBFromPositions(const Vector3* positions, size_t count) {
	if (!positions || count == 0) { m_aabbValid = false; return; }

	Vector3 mn(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t i = 0; i < count; ++i) {
		const Vector3& p = positions[i];
		if (!isFinite3(p)) continue; // defensive; skip bad verts
		mn = compMin(mn, p);
		mx = compMax(mx, p);
	}

	// If everything was non-finite, keep invalid
	if (mn.m_x > mx.m_x || mn.m_y > mx.m_y || mn.m_z > mx.m_z) {
		m_aabbValid = false; return;
	}

	m_localAABB.center = Vector3(
		(mn.m_x + mx.m_x) * 0.5f,
		(mn.m_y + mx.m_y) * 0.5f,
		(mn.m_z + mx.m_z) * 0.5f);

	m_localAABB.extents = Vector3(
		(mx.m_x - mn.m_x) * 0.5f,
		(mx.m_y - mn.m_y) * 0.5f,
		(mx.m_z - mn.m_z) * 0.5f);

	m_aabbValid = true;

}

void MeshCPU::buildLocalAABBFromMeshBuffer()
{
	// Access PositionBufferCPU through the handle
	if (!m_hPositionBufferCPU.isValid()) { m_aabbValid = false; return; }
	PositionBufferCPU* posBuf = m_hPositionBufferCPU.getObject<PositionBufferCPU>();
	if (!posBuf) { m_aabbValid = false; return; }

	// Position data is stored as flat array of floats (x,y,z,x,y,z,...)
	// Each vertex takes 3 consecutive floats
	const PrimitiveTypes::Float32* floatData = posBuf->m_values.getFirstPtr();
	size_t vertexCount = posBuf->m_values.m_size / 3;
	
	if (vertexCount == 0) { m_aabbValid = false; return; }

	Vector3 mn(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t i = 0; i < vertexCount; ++i) {
		// Extract x, y, z coordinates from the flat array
		Vector3 p(
			floatData[i * 3 + 0],  // x
			floatData[i * 3 + 1],  // y
			floatData[i * 3 + 2]   // z
		);
		
		if (!isFinite3(p)) continue; // defensive; skip bad verts
		mn = compMin(mn, p);
		mx = compMax(mx, p);
	}

	// If everything was non-finite, keep invalid
	if (mn.m_x > mx.m_x || mn.m_y > mx.m_y || mn.m_z > mx.m_z) {
		m_aabbValid = false; return;
	}

	m_localAABB.center = Vector3(
		(mn.m_x + mx.m_x) * 0.5f,
		(mn.m_y + mx.m_y) * 0.5f,
		(mn.m_z + mx.m_z) * 0.5f);

	m_localAABB.extents = Vector3(
		(mx.m_x - mn.m_x) * 0.5f,
		(mx.m_y - mn.m_y) * 0.5f,
		(mx.m_z - mn.m_z) * 0.5f);

	m_aabbValid = true;
}

void MeshCPU::createAABBDebugLines(Vector3* lineData, int& lineCount) const
{
	if (!m_aabbValid) {
		lineCount = 0;
		return;
	}

	// AABB has 8 corners, we need 12 lines to draw a wireframe box
	// DebugRenderer format: Vector3 pairs [position, color] for each line segment
	const int maxLines = 12;
	const int pointsPerLine = 2; // start and end point
	const int totalPoints = maxLines * pointsPerLine; // 24 points
	
	if (lineCount < totalPoints) {
		lineCount = 0;
		return;
	}

	Vector3 min = m_localAABB.min();
	Vector3 max = m_localAABB.max();
	
	// Define the 8 corners of the AABB
	Vector3 corners[8] = {
		Vector3(min.m_x, min.m_y, min.m_z), // 0: min corner
		Vector3(max.m_x, min.m_y, min.m_z), // 1: min corner + x
		Vector3(max.m_x, max.m_y, min.m_z), // 2: min corner + x + y
		Vector3(min.m_x, max.m_y, min.m_z), // 3: min corner + y
		Vector3(min.m_x, min.m_y, max.m_z), // 4: min corner + z
		Vector3(max.m_x, min.m_y, max.m_z), // 5: min corner + x + z
		Vector3(max.m_x, max.m_y, max.m_z), // 6: max corner
		Vector3(min.m_x, max.m_y, max.m_z)  // 7: min corner + y + z
	};

	// Define the 12 lines (each line connects 2 corners)
	int lineIndices[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, // bottom face
		{4,5}, {5,6}, {6,7}, {7,4}, // top face
		{0,4}, {1,5}, {2,6}, {3,7}  // vertical edges
	};

	// Debug color (bright yellow)
	Vector3 color(1.0f, 1.0f, 0.0f);

	int pointIndex = 0;
	for (int i = 0; i < 12; i++) {
		Vector3 start = corners[lineIndices[i][0]];
		Vector3 end = corners[lineIndices[i][1]];
		
		// Each line segment: [position, color] pair
		lineData[pointIndex++] = start;  // position
		lineData[pointIndex++] = color;  // color
		lineData[pointIndex++] = end;    // position
		lineData[pointIndex++] = color;  // color
	}
	
	lineCount = totalPoints; // 24 points (12 lines * 2 points per line)
}

}; // namespace PE

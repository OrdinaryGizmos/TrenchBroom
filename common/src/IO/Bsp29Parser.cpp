/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Bsp29Parser.h"

#include "Assets/EntityModel.h"
#include "Assets/Material.h"
#include "Exceptions.h"
#include "IO/File.h"
#include "IO/ReadMipTexture.h"
#include "IO/Reader.h"
#include "IO/ResourceUtils.h"
#include "Logger.h"
#include "Renderer/PrimType.h"
#include "Renderer/TexturedIndexRangeMap.h"
#include "Renderer/TexturedIndexRangeMapBuilder.h"

#include "kdl/result.h"
#include "kdl/string_format.h"

#include <fmt/format.h>

#include <string>
#include <vector>

namespace TrenchBroom::IO
{
namespace BspLayout
{
static const size_t DirTexturesAddress = 0x14;
// static const size_t DirTexturesSize       = 0x18;
static const size_t DirVerticesAddress = 0x1C;
// static const size_t DirVerticesSize       = 0x20;
static const size_t DirTexInfosAddress = 0x34;
// static const size_t DirTexInfoSize        = 0x38;
static const size_t DirFacesAddress = 0x3C;
// static const size_t DirFacesSize          = 0x40;
static const size_t DirEdgesAddress = 0x64;
// static const size_t DirEdgesSize          = 0x68;
static const size_t DirFaceEdgesAddress = 0x6C;
// static const size_t DirFaceEdgesSize      = 0x70;
static const size_t DirModelAddress = 0x74;
// static const size_t DirModelSize          = 0x78;

// static const size_t TextureNameLength     = 0x10;

static const size_t FaceSize = 0x14;
static const size_t FaceEdgeIndex = 0x4;
static const size_t FaceRest = 0x8;

static const size_t TexInfoSize = 0x28;
static const size_t TexInfoRest = 0x4;

static const size_t FaceEdgeSize = 0x4;
static const size_t ModelSize = 0x40;
// static const size_t ModelOrigin           = 0x18;
static const size_t ModelFaceIndex = 0x38;
// static const size_t ModelFaceCount        = 0x3c;
} // namespace BspLayout

namespace
{

struct TextureInfo
{
  vm::vec3f sAxis;
  vm::vec3f tAxis;
  float sOffset;
  float tOffset;
  size_t textureIndex;
};

struct EdgeInfo
{
  size_t vertexIndex1, vertexIndex2;
};

struct FaceInfo
{
  size_t edgeIndex;
  size_t edgeCount;
  size_t textureInfoIndex;
};


std::vector<Assets::Texture> parseTextures(
  Reader reader, const Assets::Palette& palette, const FileSystem& fs, Logger& logger)
{
  const auto textureCount = reader.readSize<int32_t>();
  auto result = std::vector<Assets::Texture>{};
  result.reserve(textureCount);

  for (size_t i = 0; i < textureCount; ++i)
  {
    const auto textureOffset = reader.readInt<int32_t>();
    // 2153: Some BSPs contain negative texture offsets.
    if (textureOffset < 0)
    {
      result.push_back(loadDefaultTexture(fs, "unknown", logger));
      continue;
    }

    const auto textureName = readMipTextureName(reader);
    auto textureReader = reader.subReaderFromBegin(size_t(textureOffset)).buffer();

    result.push_back(readIdMipTexture(textureName, textureReader, palette)
                       .or_else(makeReadTextureErrorHandler(fs, logger))
                       .value());
  }

  return result;
}

std::vector<TextureInfo> parseTextureInfos(Reader reader, const size_t textureInfoCount)
{
  auto result = std::vector<TextureInfo>(textureInfoCount);
  for (size_t i = 0; i < textureInfoCount; ++i)
  {
    result[i].sAxis = reader.readVec<float, 3>();
    result[i].sOffset = reader.readFloat<float>();
    result[i].tAxis = reader.readVec<float, 3>();
    result[i].tOffset = reader.readFloat<float>();
    result[i].textureIndex = reader.readSize<uint32_t>();
    reader.seekForward(BspLayout::TexInfoRest);
  }
  return result;
}

std::vector<vm::vec3f> parseVertices(Reader reader, const size_t vertexCount)
{
  auto result = std::vector<vm::vec3f>(vertexCount);
  for (size_t i = 0; i < vertexCount; ++i)
  {
    result[i] = reader.readVec<float, 3>();
  }
  return result;
}

std::vector<EdgeInfo> parseEdgeInfos(Reader reader, const size_t edgeInfoCount)
{
  auto result = std::vector<EdgeInfo>(edgeInfoCount);
  for (size_t i = 0; i < edgeInfoCount; ++i)
  {
    result[i].vertexIndex1 = reader.readSize<uint16_t>();
    result[i].vertexIndex2 = reader.readSize<uint16_t>();
  }
  return result;
}

std::vector<FaceInfo> parseFaceInfos(Reader reader, const size_t faceInfoCount)
{
  auto result = std::vector<FaceInfo>(faceInfoCount);
  for (size_t i = 0; i < faceInfoCount; ++i)
  {
    reader.seekForward(BspLayout::FaceEdgeIndex);
    result[i].edgeIndex = reader.readSize<int32_t>();
    result[i].edgeCount = reader.readSize<uint16_t>();
    result[i].textureInfoIndex = reader.readSize<uint16_t>();
    reader.seekForward(BspLayout::FaceRest);
  }
  return result;
}

std::vector<int> parseFaceEdges(Reader reader, const size_t faceEdgeCount)
{
  auto result = std::vector<int>(faceEdgeCount);
  for (size_t i = 0; i < faceEdgeCount; ++i)
  {
    result[i] = reader.readInt<int32_t>();
  }
  return result;
}

vm::vec2f textureCoords(
  const vm::vec3f& vertex, const TextureInfo& textureInfo, const Assets::Texture* texture)
{
  return texture ? vm::vec2f{
           (vm::dot(vertex, textureInfo.sAxis) + textureInfo.sOffset)
             / float(texture->width()),
           (vm::dot(vertex, textureInfo.tAxis) + textureInfo.tOffset)
             / float(texture->height())}
                 : vm::vec2f::zero();
}

void parseFrame(
  Reader reader,
  const size_t frameIndex,
  Assets::EntityModel& model,
  const std::vector<TextureInfo>& textureInfos,
  const std::vector<vm::vec3f>& vertices,
  const std::vector<EdgeInfo>& edgeInfos,
  const std::vector<FaceInfo>& faceInfos,
  const std::vector<int>& faceEdges)
{
  using Vertex = Assets::EntityModelVertex;

  auto& surface = model.surface(0);

  reader.seekForward(BspLayout::ModelFaceIndex);
  const auto modelFaceIndex = reader.readSize<int32_t>();
  const auto modelFaceCount = reader.readSize<int32_t>();
  auto totalVertexCount = size_t(0);
  auto size = Renderer::TexturedIndexRangeMap::Size{};

  for (size_t i = 0; i < modelFaceCount; ++i)
  {
    const auto& faceInfo = faceInfos[modelFaceIndex + i];
    const auto& textureInfo = textureInfos[faceInfo.textureInfoIndex];
    if (const auto* skin = surface.skin(textureInfo.textureIndex))
    {
      const auto faceVertexCount = faceInfo.edgeCount;
      size.inc(skin, Renderer::PrimType::Polygon, faceVertexCount);
      totalVertexCount += faceVertexCount;
    }
  }

  auto bounds = vm::bbox3f::builder{};

  auto builder =
    Renderer::TexturedIndexRangeMapBuilder<Vertex::Type>{totalVertexCount, size};
  for (size_t i = 0; i < modelFaceCount; ++i)
  {
    const auto& faceInfo = faceInfos[modelFaceIndex + i];
    const auto& textureInfo = textureInfos[faceInfo.textureInfoIndex];
    if (const auto* skin = surface.skin(textureInfo.textureIndex))
    {
      const auto faceVertexCount = faceInfo.edgeCount;

      auto faceVertices = std::vector<Vertex>{};
      faceVertices.reserve(faceVertexCount);

      for (size_t k = 0; k < faceVertexCount; ++k)
      {
        const auto faceEdgeIndex = faceEdges[faceInfo.edgeIndex + k];
        const auto vertexIndex = faceEdgeIndex < 0
                                   ? edgeInfos[size_t(-faceEdgeIndex)].vertexIndex2
                                   : edgeInfos[size_t(faceEdgeIndex)].vertexIndex1;

        const auto& position = vertices[vertexIndex];
        const auto texCoords = textureCoords(position, textureInfo, skin);

        bounds.add(position);

        faceVertices.emplace_back(position, texCoords);
      }

      builder.addPolygon(skin, faceVertices);
    }
  }

  const auto frameName = fmt::format("{}_{}", model.name(), frameIndex);
  auto& frame = model.loadFrame(frameIndex, frameName, bounds.bounds());
  surface.addTexturedMesh(
    frame, std::move(builder.vertices()), std::move(builder.indices()));
}

} // namespace

Bsp29Parser::Bsp29Parser(
  std::string name, const Reader& reader, Assets::Palette palette, const FileSystem& fs)
  : m_name{std::move(name)}
  , m_reader{reader}
  , m_palette{std::move(palette)}
  , m_fs{fs}
{
}

bool Bsp29Parser::canParse(const std::filesystem::path& path, Reader reader)
{
  if (kdl::str_to_lower(path.extension().string()) != ".bsp")
  {
    return false;
  }

  const auto version = reader.readInt<int32_t>();
  return version == 29;
}

std::unique_ptr<Assets::EntityModel> Bsp29Parser::initializeModel(Logger& logger)
{
  auto reader = m_reader;
  const auto version = reader.readInt<int32_t>();
  if (version != 29)
  {
    throw AssetException("Unsupported BSP model version: " + std::to_string(version));
  }

  reader.seekFromBegin(BspLayout::DirTexturesAddress);
  const auto textureOffset = reader.readSize<int32_t>();

  reader.seekFromBegin(BspLayout::DirModelAddress);
  /* const auto modelsOffset = */ reader.readSize<int32_t>();
  const auto modelsLength = reader.readSize<int32_t>();
  const auto frameCount = modelsLength / BspLayout::ModelSize;

  auto textures =
    parseTextures(reader.subReaderFromBegin(textureOffset), m_palette, m_fs, logger);

  auto model = std::make_unique<Assets::EntityModel>(
    m_name, Assets::PitchType::Normal, Assets::Orientation::Oriented);
  for (size_t i = 0; i < frameCount; ++i)
  {
    model->addFrame();
  }

  auto& surface = model->addSurface(m_name);
  surface.setSkins(std::move(textures));

  return model;
}

void Bsp29Parser::loadFrame(
  const size_t frameIndex, Assets::EntityModel& model, Logger& /* logger */)
{
  auto reader = m_reader;
  const auto version = reader.readInt<int32_t>();
  if (version != 29)
  {
    throw AssetException("Unsupported BSP model version: " + std::to_string(version));
  }

  reader.seekFromBegin(BspLayout::DirTexturesAddress);
  /* const auto textureOffset = */ reader.readSize<int32_t>();
  /* const auto textureLength = */ reader.readSize<int32_t>();

  reader.seekFromBegin(BspLayout::DirTexInfosAddress);
  const auto textureInfoOffset = reader.readSize<int32_t>();
  const auto textureInfoLength = reader.readSize<int32_t>();
  const auto textureInfoCount = textureInfoLength / BspLayout::TexInfoSize;

  reader.seekFromBegin(BspLayout::DirVerticesAddress);
  const auto vertexOffset = reader.readSize<int32_t>();
  const auto vertexLength = reader.readSize<int32_t>();
  const auto vertexCount = vertexLength / (3 * sizeof(float));

  reader.seekFromBegin(BspLayout::DirEdgesAddress);
  const auto edgeInfoOffset = reader.readSize<int32_t>();
  const auto edgeInfoLength = reader.readSize<int32_t>();
  const auto edgeInfoCount = edgeInfoLength / (2 * sizeof(uint16_t));

  reader.seekFromBegin(BspLayout::DirFacesAddress);
  const auto faceInfoOffset = reader.readSize<int32_t>();
  const auto faceInfoLength = reader.readSize<int32_t>();
  const auto faceInfoCount = faceInfoLength / BspLayout::FaceSize;

  reader.seekFromBegin(BspLayout::DirFaceEdgesAddress);
  const auto faceEdgesOffset = reader.readSize<int32_t>();
  const auto faceEdgesLength = reader.readSize<int32_t>();
  const auto faceEdgesCount = faceEdgesLength / BspLayout::FaceEdgeSize;

  reader.seekFromBegin(BspLayout::DirModelAddress);
  const auto modelsOffset = reader.readSize<int32_t>();

  const auto textureInfos =
    parseTextureInfos(reader.subReaderFromBegin(textureInfoOffset), textureInfoCount);
  const auto vertices =
    parseVertices(reader.subReaderFromBegin(vertexOffset), vertexCount);
  const auto edgeInfos =
    parseEdgeInfos(reader.subReaderFromBegin(edgeInfoOffset), edgeInfoCount);
  const auto faceInfos =
    parseFaceInfos(reader.subReaderFromBegin(faceInfoOffset), faceInfoCount);
  const auto faceEdges =
    parseFaceEdges(reader.subReaderFromBegin(faceEdgesOffset), faceEdgesCount);

  parseFrame(
    reader.subReaderFromBegin(
      modelsOffset + frameIndex * BspLayout::ModelSize, BspLayout::ModelSize),
    frameIndex,
    model,
    textureInfos,
    vertices,
    edgeInfos,
    faceInfos,
    faceEdges);
}

} // namespace TrenchBroom::IO

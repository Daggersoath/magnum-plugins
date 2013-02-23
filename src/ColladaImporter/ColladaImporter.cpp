/*
    Copyright © 2010, 2011, 2012 Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Magnum.

    Magnum is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Magnum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

#include "ColladaImporter.h"

#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <Utility/Directory.h>
#include <Math/Constants.h>
#include <Trade/ImageData.h>
#include <Trade/MeshData3D.h>
#include <Trade/MeshObjectData3D.h>
#include <Trade/PhongMaterialData.h>
#include <Trade/SceneData.h>

#include "TgaImporter/TgaImporter.h"

using Corrade::PluginManager::AbstractPluginManager;
using Corrade::Utility::Directory;

namespace Magnum { namespace Trade { namespace ColladaImporter {

const QString ColladaImporter::namespaceDeclaration =
    "declare default element namespace \"http://www.collada.org/2005/11/COLLADASchema\";\n";

ColladaImporter::ColladaImporter(AbstractPluginManager* manager, const std::string& plugin): AbstractImporter(manager, plugin), d(0), zero(0), app(qApp ? 0 : new QCoreApplication(zero, 0)) {}

ColladaImporter::~ColladaImporter() {
    close();
    delete app;
}

ColladaImporter::Document::~Document() {
    for(auto i: scenes) delete i.second;
    for(auto i: objects) delete i.second;
    for(auto i: meshes) delete i.second;
    for(auto i: materials) delete i.second;
    for(auto i: images2D) delete i.second;
}

bool ColladaImporter::open(const std::string& filename) {
    /* Close previous document */
    if(d) close();

    QXmlQuery query;

    /* Open the file and load it into XQuery */
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        Error() << "ColladaImporter: cannot open file" << filename;
        return false;
    }
    if(!query.setFocus(&file)) {
        Error() << "ColladaImporter: cannot load XML";
        return false;
    }

    QString tmp;

    /* Check namespace */
    query.setQuery("namespace-uri(/*:COLLADA)");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "http://www.collada.org/2005/11/COLLADASchema") {
        Error() << "ColladaImporter: unsupported namespace" << ('"'+tmp+'"').toStdString();
        return false;
    }

    /* Check version */
    query.setQuery(namespaceDeclaration + "/COLLADA/@version/string()");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "1.4.1") {
        Error() << "ColladaImporter: unsupported version" << ('"'+tmp+'"').toStdString();
        return false;
    }

    d = new Document;
    d->filename = filename;
    d->query = query;

    /* Scenes */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_visual_scenes/visual_scene)");
    query.evaluateTo(&tmp);
    d->scenes.resize(ColladaType<GLuint>::fromString(tmp));

    /* Objects */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_visual_scenes/visual_scene//node)");
    query.evaluateTo(&tmp);
    d->objects.resize(ColladaType<GLuint>::fromString(tmp));

    QStringList tmpList;

    /* Create camera name -> camera id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_cameras/camera/@id/string()");
    query.evaluateTo(&tmpList);
    d->camerasForName.reserve(tmpList.size());
    for(const QString id: tmpList)
        d->camerasForName.insert(std::make_pair(id.trimmed().toStdString(), d->camerasForName.size()));

    /* Create light name -> light id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_lights/light/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->lightsForName.reserve(tmpList.size());
    for(const QString id: tmpList)
        d->lightsForName.insert(std::make_pair(id.trimmed().toStdString(), d->lightsForName.size()));

    /* Create material name -> material id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_materials/material/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->materials.reserve(tmpList.size());
    d->materialsForName.reserve(tmpList.size());
    for(const QString id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->materials.push_back({name, nullptr});
        d->materialsForName.insert({name, d->materialsForName.size()});
    }

    /* Create mesh name -> mesh id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_geometries/geometry/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->meshes.reserve(tmpList.size());
    d->meshesForName.reserve(tmpList.size());
    for(const QString id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->meshes.push_back({name, nullptr});
        d->meshesForName.insert({name, d->meshesForName.size()});
    }

    /* Create image name -> image id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_images/image/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->images2D.reserve(tmpList.size());
    d->images2DForName.reserve(tmpList.size());
    for(const QString id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->images2D.push_back({name, nullptr});
        d->images2DForName.insert({name, d->images2DForName.size()});
    }

    return true;
}

void ColladaImporter::close() {
    if(!d) return;

    delete d;
    d = 0;
}

std::int32_t ColladaImporter::ColladaImporter::defaultScene() {
    if(!d || d->scenes.empty()) return -1;
    if(!d->scenes[0].second) parseScenes();

    return d->defaultScene;
}

std::string ColladaImporter::ColladaImporter::sceneName(uint32_t id) {
    if(!d || id >= d->scenes.size()) return {};
    if(!d->scenes[0].second) parseScenes();

    return d->scenes[id].first;
}

SceneData* ColladaImporter::ColladaImporter::scene(std::uint32_t id) {
    if(!d || id >= d->scenes.size()) return nullptr;
    if(!d->scenes[0].second) parseScenes();

    return d->scenes[id].second;
}

std::int32_t ColladaImporter::ColladaImporter::object3DForName(const std::string& name) {
    if(d->scenes.empty()) return -1;
    if(!d->scenes[0].second) parseScenes();

    auto it = d->objectsForName.find(name);
    return (it == d->objectsForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::ColladaImporter::object3DName(std::uint32_t id) {
    if(!d || id >= d->objects.size()) return {};
    if(!d->scenes[0].second) parseScenes();

    return d->objects[id].first;
}

ObjectData3D* ColladaImporter::ColladaImporter::object3D(std::uint32_t id) {
    if(!d || id >= d->objects.size()) return nullptr;
    if(!d->scenes[0].second) parseScenes();

    return d->objects[id].second;
}

std::int32_t ColladaImporter::ColladaImporter::mesh3DForName(const std::string& name) {
    auto it = d->meshesForName.find(name);
    return (it == d->meshesForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::ColladaImporter::mesh3DName(uint32_t id) {
    if(!d || id >= d->meshes.size()) return {};
    return d->meshes[id].first;
}

MeshData3D* ColladaImporter::mesh3D(std::uint32_t id) {
    if(!d || id >= d->meshes.size()) return nullptr;
    if(d->meshes[id].second) return d->meshes[id].second;

    /** @todo More polylists in one mesh */

    /* Get polygon count */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/@count/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint polygonCount = ColladaType<GLuint>::fromString(tmp);

    /* Get vertex count per polygon */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/vcount/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    std::vector<GLuint> vertexCountPerFace = Utility::parseArray<GLuint>(tmp, polygonCount);

    GLuint vertexCount = 0;
    std::vector<GLuint> quads;
    for(std::size_t i = 0; i != vertexCountPerFace.size(); ++i) {
        GLuint count = vertexCountPerFace[i];

        if(count == 4) quads.push_back(i);
        else if(count != 3) {
            Error() << "ColladaImporter:" << count << "vertices per face not supported";
            return nullptr;
        }

        vertexCount += count;
    }

    /* Get input count per vertex */
    d->query.setQuery((namespaceDeclaration + "count(/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input)").arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint stride = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh indices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/p/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    std::vector<GLuint> originalIndices = Utility::parseArray<GLuint>(tmp, vertexCount*stride);

    /** @todo assert size()%stride == 0 */

    /* Get unique combinations of vertices, build resulting index array. Key
       is position of unique index combination from original vertex array,
       value is resulting index. */
    std::unordered_map<std::uint32_t, std::uint32_t, IndexHash, IndexEqual> indexCombinations(originalIndices.size()/stride, IndexHash(originalIndices, stride), IndexEqual(originalIndices, stride));
    std::vector<std::uint32_t> combinedIndices;
    for(std::size_t i = 0, end = originalIndices.size()/stride; i != end; ++i)
        combinedIndices.push_back(indexCombinations.insert(std::make_pair(i, indexCombinations.size())).first->second);

    /* Convert quads to triangles */
    std::vector<std::uint32_t>* indices = new std::vector<std::uint32_t>;
    std::size_t quadId = 0;
    for(std::size_t i = 0; i != vertexCountPerFace.size(); ++i) {
        if(quads.size() > quadId && quads[quadId] == i) {
            indices->push_back(combinedIndices[i*3+quadId]);
            indices->push_back(combinedIndices[i*3+quadId+1]);
            indices->push_back(combinedIndices[i*3+quadId+2]);
            indices->push_back(combinedIndices[i*3+quadId]);
            indices->push_back(combinedIndices[i*3+quadId+2]);
            indices->push_back(combinedIndices[i*3+quadId+3]);

            ++quadId;
        } else {
            indices->push_back(combinedIndices[i*3+quadId]);
            indices->push_back(combinedIndices[i*3+quadId+1]);
            indices->push_back(combinedIndices[i*3+quadId+2]);
        }
    }

    /* Get mesh vertices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='VERTEX']/@source/string()")
        .arg(id+1));
    d->query.evaluateTo(&tmp);
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/vertices[@id='%0']/input[@semantic='POSITION']/@source/string()")
        .arg(tmp.mid(1).trimmed()));
    d->query.evaluateTo(&tmp);
    std::vector<Vector3> originalVertices = parseSource<Vector3>(tmp.mid(1).trimmed());

    /* Build vertex array */
    GLuint vertexOffset = attributeOffset(id, "VERTEX");
    auto vertices = new std::vector<Vector3>(indexCombinations.size());
    for(auto i: indexCombinations)
        (*vertices)[i.second] = originalVertices[originalIndices[i.first*stride+vertexOffset]];

    QStringList tmpList;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input/@semantic/string()").arg(id+1));
    d->query.evaluateTo(&tmpList);
    std::vector<std::vector<Vector3>*> normals;
    std::vector<std::vector<Vector2>*> textureCoords2D;
    for(QString attribute: tmpList) {
        /* Vertices - already built */
        if(attribute == "VERTEX") continue;

        /* Normals */
        else if(attribute == "NORMAL")
            normals.push_back(buildAttributeArray<Vector3>(id, "NORMAL", normals.size(), originalIndices, stride, indexCombinations));

        /* 2D texture coords */
        else if(attribute == "TEXCOORD")
            textureCoords2D.push_back(buildAttributeArray<Vector2>(id, "TEXCOORD", textureCoords2D.size(), originalIndices, stride, indexCombinations));

        /* Something other */
        else Warning() << "ColladaImporter:" << '"' + attribute.toStdString() + '"' << "input semantic not supported";
    }

    return d->meshes[id].second = new MeshData3D(Mesh::Primitive::Triangles, indices, {vertices}, normals, textureCoords2D);
}

std::int32_t ColladaImporter::ColladaImporter::materialForName(const std::string& name) {
    auto it = d->materialsForName.find(name);
    return (it == d->materialsForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::ColladaImporter::materialName(std::uint32_t id) {
    if(!d || id >= d->materials.size()) return {};
    return d->materials[id].first;
}

AbstractMaterialData* ColladaImporter::material(std::uint32_t id) {
    if(!d || id >= d->materials.size()) return nullptr;
    if(d->materials[id].second) return d->materials[id].second;

    /* Get effect ID */
    QString effect;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_materials/material[%0]/instance_effect/@url/string()").arg(id+1));
    d->query.evaluateTo(&effect);
    effect = effect.mid(1).trimmed();

    /* Find out which profile it is */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/*[substring(name(), 1, 8) = 'profile_']/name()").arg(effect));
    d->query.evaluateTo(&tmp);

    /** @todo Support other profiles */

    if(tmp.trimmed() != "profile_COMMON") {
        Error() << "ColladaImporter:" << ('"'+tmp.trimmed()+'"').toStdString() << "effect profile not supported";
        return nullptr;
    }

    /* Get shader type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/*/name()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /** @todo Other (blinn, goraund) profiles */
    if(tmp != "phong") {
        Error() << "ColladaImporter:" << ('"'+tmp+'"').toStdString() << "shader not supported";
        return nullptr;
    }

    /* Ambient color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/ambient/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 ambientColor = Utility::parseVector<Vector3>(tmp);

    /* Diffuse color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/diffuse/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 diffuseColor = Utility::parseVector<Vector3>(tmp);

    /* Specular color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/specular/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 specularColor = Utility::parseVector<Vector3>(tmp);

    /* Shininess */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/shininess/float/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    GLfloat shininess = ColladaType<GLfloat>::fromString(tmp);

    /** @todo Emission, IOR */

    return d->materials[id].second = new PhongMaterialData(ambientColor, diffuseColor, specularColor, shininess);
}

std::int32_t ColladaImporter::ColladaImporter::image2DForName(const std::string& name) {
    auto it = d->images2DForName.find(name);
    return (it == d->images2DForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::ColladaImporter::image2DName(uint32_t id) {
    if(!d || id >= d->images2D.size()) return {};
    return d->images2D[id].first;
}

ImageData2D* ColladaImporter::image2D(std::uint32_t id) {
    if(!d || id >= d->images2D.size()) return nullptr;
    if(d->images2D[id].second) return d->images2D[id].second;

    /* Image filename */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_images/image[%0]/init_from/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    if(tmp.right(3) != "tga") {
        Error() << "ColladaImporter:" << '"' + tmp.toStdString() + '"' << "has unsupported format";
        return nullptr;
    }

    TgaImporter::TgaImporter tgaImporter;
    ImageData2D* image;
    if(!tgaImporter.open(Directory::join(Directory::path(d->filename), tmp.toStdString())) || !(image = tgaImporter.image2D(0)))
        return nullptr;

    return d->images2D[id].second = image;
}

GLuint ColladaImporter::attributeOffset(std::uint32_t meshId, const QString& attribute, std::uint32_t id) {
    QString tmp;

    /* Get attribute offset in indices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@offset/string()")
        .arg(meshId+1).arg(attribute).arg(id+1));
    d->query.evaluateTo(&tmp);
    return ColladaType<GLuint>::fromString(tmp);
}

void ColladaImporter::parseScenes() {
    QStringList tmpList;
    QString tmp;

    /* Default scene */
    d->defaultScene = 0;
    d->query.setQuery(namespaceDeclaration + "/COLLADA/scene/instance_visual_scene/@url/string()");
    d->query.evaluateTo(&tmp);
    std::string defaultScene = tmp.trimmed().mid(1).toStdString();

    /* Parse all objects in all scenes */
    for(std::uint32_t sceneId = 0; sceneId != d->scenes.size(); ++sceneId) {
        /* Is this the default scene? */
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene[%0]/@id/string()").arg(sceneId+1));
        d->query.evaluateTo(&tmp);
        std::string name = tmp.trimmed().toStdString();
        if(defaultScene == name)
            d->defaultScene = sceneId;

        std::uint32_t nextObjectId = 0;
        std::vector<std::uint32_t> children;
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene[%0]/node/@id/string()").arg(sceneId+1));
        tmpList.clear();
        d->query.evaluateTo(&tmpList);
        for(QString childId: tmpList) {
            children.push_back(nextObjectId);
            nextObjectId = parseObject(nextObjectId, childId.trimmed());
        }

        d->scenes[sceneId] = {name, new SceneData({}, children)};
    }
}

std::uint32_t ColladaImporter::parseObject(std::uint32_t id, const QString& name) {
    QString tmp;
    QStringList tmpList, tmpList2;

    /* Transformations */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/(translate|rotate|scale)/name()").arg(name));
    d->query.evaluateTo(&tmpList);

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/(translate|rotate|scale)/string()").arg(name));
    d->query.evaluateTo(&tmpList2);

    Matrix4 transformation;
    for(std::size_t i = 0; i != std::size_t(tmpList.size()); ++i) {
        QString type = tmpList[i].trimmed();
        /* Translation */
        if(type == "translate")
            transformation = transformation*Matrix4::translation(Utility::parseVector<Vector3>(tmpList2[i]));

        /* Rotation */
        else if(type == "rotate") {
            int pos = 0;
            Vector3 axis = Utility::parseVector<Vector3>(tmpList2[i], &pos);
            Deg angle(ColladaType<GLfloat>::fromString(tmpList2[i].mid(pos)));
            transformation = transformation*Matrix4::rotation(angle, axis);

        /* Scaling */
        } else if(type == "scale")
            transformation = transformation*Matrix4::scaling(Utility::parseVector<Vector3>(tmpList2[i]));

        /* It shouldn't get here */
        else CORRADE_ASSERT(0, ("ColladaImporter: unknown translation " + type).toStdString(), id);
    }

    /* Instance type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/*[substring(name(), 1, 9) = 'instance_']/name()").arg(name));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /* Camera instance */
    if(tmp == "instance_camera") {
        std::string cameraName = instanceName(name, "instance_camera");
        auto cameraId = d->camerasForName.find(cameraName);
        if(cameraId == d->camerasForName.end()) {
            Error() << "ColladaImporter: camera" << '"'+cameraName+'"' << "was not found";
            return id;
        }

        d->objects[id] = {name.toStdString(), new ObjectData3D({}, transformation, ObjectData3D::InstanceType::Camera, cameraId->second)};

    /* Light instance */
    } else if(tmp == "instance_light") {
        std::string lightName = instanceName(name, "instance_light");
        auto lightId = d->lightsForName.find(lightName);
        if(lightId == d->lightsForName.end()) {
            Error() << "ColladaImporter: light" << '"'+lightName+'"' << "was not found";
            return id;
        }

        d->objects[id] = {name.toStdString(), new ObjectData3D({}, transformation, ObjectData3D::InstanceType::Light, lightId->second)};

    /* Mesh instance */
    } else if(tmp == "instance_geometry") {
        std::string meshName = instanceName(name, "instance_geometry");
        auto meshId = d->meshesForName.find(meshName);
        if(meshId == d->meshesForName.end()) {
            Error() << "ColladaImporter: mesh" << '"'+meshName+'"' << "was not found";
            return id;
        }

        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/instance_geometry/bind_material/technique_common/instance_material/@target/string()").arg(name));
        d->query.evaluateTo(&tmp);
        std::string materialName = tmp.trimmed().mid(1).toStdString();

        /* Mesh doesn't have bound material, add default one */
        /** @todo Solution for unknown materials etc.: -1 ? */
        if(materialName.empty()) d->objects[id] = {name.toStdString(), new MeshObjectData3D({}, transformation, meshId->second, 0)};

        /* Else find material ID */
        else {
            auto materialId = d->materialsForName.find(materialName);
            if(materialId == d->materialsForName.end()) {
                Error() << "ColladaImporter: material" << '"'+materialName+'"' << "was not found";
                return id;
            }

            d->objects[id] = {name.toStdString(), new MeshObjectData3D({}, transformation, meshId->second, materialId->second)};
        }

    /* Blender group instance */
    } else if(tmp.isEmpty()) {
        d->objects[id] = {name.toStdString(), new ObjectData3D({}, transformation)};

    } else {
        Error() << "ColladaImporter:" << '"'+tmp.toStdString()+'"' << "instance type not supported";
        return id;
    }

    /* Add to object name map */
    d->objectsForName.insert({name.toStdString(), id});

    /* Parse child objects */
    std::uint32_t nextObjectId = id+1;
    std::vector<std::uint32_t> children;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/node/@id/string()").arg(name));
    tmpList.clear();
    d->query.evaluateTo(&tmpList);
    for(QString childId: tmpList) {
        children.push_back(nextObjectId);
        nextObjectId = parseObject(nextObjectId, childId.trimmed());
    }
    d->objects[id].second->children().swap(children);

    return nextObjectId;
}

std::string ColladaImporter::instanceName(const QString& name, const QString& instanceTag) {
    QString tmp;

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/%1/@url/string()").arg(name).arg(instanceTag));
    d->query.evaluateTo(&tmp);
    return tmp.trimmed().mid(1).toStdString();
}

}}}

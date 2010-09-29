/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2010 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include <osgEarth/EarthFile>
#include <osgEarth/XmlUtils>
#include <osgEarth/HTTPClient>
#include <osgEarth/Registry>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <OpenThreads/ScopedLock>

using namespace osgEarth;
using namespace OpenThreads;

EarthFile::EarthFile()
{
    //nop
}

EarthFile::EarthFile( Map* map, const MapNodeOptions& mapOptions ) :
_map( map ),
_mapNodeOptions( mapOptions )
{
    //nop
}

void
EarthFile::setMap( Map* map ) {
    _map = map;
}

void
EarthFile::setMapNodeOptions( const MapNodeOptions& mapOptions ) {
    _mapNodeOptions = mapOptions;
}

const Map*
EarthFile::getMap() const {
    return _map.get();
}

Map*
EarthFile::getMap() {
    return _map.get();
}

const MapNodeOptions& 
EarthFile::getMapNodeOptions() const {
    return _mapNodeOptions;
}

MapNodeOptions& 
EarthFile::getMapNodeOptions() {
    return _mapNodeOptions;
}

#define ELEM_MAP                      "map"
#define ATTR_NAME                     "name"
#define ATTR_CSTYPE                   "type"
#define ELEM_MAP_OPTIONS              "options"
#define ELEM_TERRAIN_OPTIONS          "terrain"
#define ELEM_IMAGE                    "image"
#define ELEM_HEIGHTFIELD              "heightfield"
#define ATTR_MIN_LEVEL                "min_level"
#define ATTR_MAX_LEVEL                "max_level"
#define ELEM_CACHE                    "cache"
#define ATTR_TYPE                     "type"
#define ELEM_NODATA_IMAGE             "nodata_image"
#define ELEM_TRANSPARENT_COLOR        "transparent_color"
#define ELEM_CACHE_FORMAT             "cache_format"
#define ELEM_CACHE_ENABLED            "cache_enabled"
#define ELEM_MODEL                    "model"
//#define ELEM_MASK_MODEL               "mask_model"
#define ELEM_MASK                     "mask"
#define ELEM_OPACITY                  "opacity"
#define ELEM_ENABLED                  "enabled"

#define VALUE_TRUE                    "true"
#define VALUE_FALSE                   "false"

#define ELEM_PROFILE                  "profile"
#define ATTR_MINX                     "xmin"
#define ATTR_MINY                     "ymin"
#define ATTR_MAXX                     "xmax"
#define ATTR_MAXY                     "ymax"
#define ATTR_SRS                      "srs"

#define ATTR_LOADING_WEIGHT           "loading_weight"


static ModelLayer*
readModelLayer( const Config& conf )
{
    ModelLayer* layer = new ModelLayer( conf.value("name"), ModelSourceOptions(conf) );
    return layer;
}

static MaskLayer*
readMaskLayer( const Config& conf )
{
    MaskLayer* layer = new MaskLayer( ModelSourceOptions(conf) );
    return layer;
}

static MapLayer*
readMapLayer( const Config& conf, const Config& additional )
{
    // divine layer type
    MapLayer::Type layerType =
        conf.key() == ELEM_HEIGHTFIELD ? MapLayer::TYPE_HEIGHTFIELD :
        MapLayer::TYPE_IMAGE;

    Config driverConf = conf;
    driverConf.add( additional.children() );

    MapLayer* layer = new MapLayer( conf.value("name"), layerType, TileSourceOptions(driverConf) );

    return layer;
}


static Config
writeLayer( ModelLayer* layer, const std::string& typeName ="" )
{
    Config conf = layer->getConfig();
    conf.key() = !typeName.empty() ? typeName : ELEM_MODEL;
    return conf;
}

static Config
writeLayer( MaskLayer* layer )
{
    Config conf = layer->getConfig();
    conf.key() = "mask";
    return conf;
}

static bool
readMap( const Config& conf, const std::string& referenceURI, EarthFile* earth )
{
//    OE_NOTICE << conf.toString() << std::endl;

    bool success = true;

    MapOptions mapOptions( conf.child( ELEM_MAP_OPTIONS ) );

    // legacy: check for name/type in top-level attrs:
    if ( conf.hasValue( "name" ) || conf.hasValue( "type" ) )
    {
        Config legacy;
        if ( conf.hasValue("name") ) legacy.add( "name", conf.value("name") );
        if ( conf.hasValue("type") ) legacy.add( "type", conf.value("type") );
        mapOptions.mergeConfig( legacy );
    }

    // setting the reference URI will support relative paths in the earth file.
    mapOptions.referenceURI() = referenceURI;

    //std::string a_cstype = conf.value( ATTR_CSTYPE );
    //if ( a_cstype == "geocentric" || a_cstype == "round" || a_cstype == "globe" || a_cstype == "earth" )
    //    cstype = Map::CSTYPE_GEOCENTRIC;
    //else if ( a_cstype == "geographic" || a_cstype == "flat" || a_cstype == "plate carre" || a_cstype == "projected")
    //    cstype = Map::CSTYPE_PROJECTED;
    //else if ( a_cstype == "cube" )
    //    cstype = Map::CSTYPE_GEOCENTRIC_CUBE;

    osg::ref_ptr<Map> map = new Map( mapOptions );
    //map->setReferenceURI( referenceURI );
    //map->setName( conf.value( ATTR_NAME ) );

    MapNodeOptions mapNodeOptions( conf.child( ELEM_MAP_OPTIONS ) );

    //if ( conf.hasChild( ELEM_MAP_OPTIONS ) )
    //    mapOptions = MapNodeOptions( conf.child( ELEM_MAP_OPTIONS ) );
    //else
    //    mapOptions = MapNodeOptions( conf ); // old style, for backwards compatibility  

    //NOTE: this is now in MapNodeOptions
    ////Read the profile definition
    //if ( conf.hasChild( ELEM_PROFILE ) )
    //    map->profileOptions() = ProfileOptions( conf.child( ELEM_PROFILE ) );

    //NOTE: this is now in MapOptions
    //Try to read the global map cache if one is specifiec
  //  if ( conf.hasChild( ELEM_CACHE ) )
  //  {
  //      map->cacheConfig() = CacheConfig( conf.child( ELEM_CACHE ) );

		////Create and set the Cache for the Map
		//CacheFactory factory;
  //      Cache* cache = factory.create( map->cacheConfig().value() );
  //      if ( cache )
		//    map->setCache( cache );
  //  }

	if ( osgEarth::Registry::instance()->getCacheOverride() )
	{
		OE_NOTICE << "Overriding map cache with global cache override" << std::endl;
		map->setCache( osgEarth::Registry::instance()->getCacheOverride() );
	}

    // Read the layers in LAST (otherwise they will not benefit from the cache/profile configuration)
    ConfigSet images = conf.children( ELEM_IMAGE );
    for( ConfigSet::const_iterator i = images.begin(); i != images.end(); i++ )
    {
        Config additional;
        additional.add( "default_tile_size", "256" );

        MapLayer* layer = readMapLayer( *i, additional );
        if ( layer )
            map->addMapLayer( layer );
    }

    ConfigSet heightfields = conf.children( ELEM_HEIGHTFIELD );
    for( ConfigSet::const_iterator i = heightfields.begin(); i != heightfields.end(); i++ )
    {
        Config additional;
        additional.add( "default_tile_size", "16" );

        MapLayer* layer = readMapLayer( *i, additional );
        if ( layer )
            map->addMapLayer( layer );
    }

    ConfigSet models = conf.children( ELEM_MODEL );
    for( ConfigSet::const_iterator i = models.begin(); i != models.end(); i++ )
    {
        ModelLayer* layer = readModelLayer( *i );
        if ( layer )
            map->addModelLayer( layer );
    }

    Config maskLayerConf = conf.child( ELEM_MASK );
    if ( !maskLayerConf.empty() )
    {
        MaskLayer* layer = readMaskLayer( maskLayerConf );
        if ( layer )
            map->setTerrainMaskLayer( layer );
    }

    earth->setMap( map.get() );
    earth->setMapNodeOptions( mapNodeOptions );

    return success;
}


static Config
getConfig( Map* map, const MapNodeOptions& ep )
{
    Config conf( ELEM_MAP );
    
    //conf.attr( ATTR_NAME ) = map->getName();
    
    // merge the MapOptions and MapNodeOptions together under <options>:
    Config optionsConf( ELEM_MAP_OPTIONS );
    optionsConf.merge( map->getMapOptions().getConfig() );
    optionsConf.merge( ep.getConfig() );
    conf.add( optionsConf );

    ////Write the coordinate system
    //std::string cs;
    //if (map->getCoordinateSystemType() == Map::CSTYPE_GEOCENTRIC) cs = "geocentric";
    //else if (map->getCoordinateSystemType() == Map::CSTYPE_PROJECTED) cs = "projected";
    //else if ( map->getCoordinateSystemType() == Map::CSTYPE_GEOCENTRIC_CUBE) cs = "cube";
    //else
    //{
    //    OE_NOTICE << "[osgEarth::EarthFile] Unhandled CoordinateSystemType " << std::endl;
    //    return Config();
    //}
    //conf.attr( ATTR_CSTYPE ) = cs;

    //Write all the image sources
    for( MapLayerList::const_iterator i = map->getImageMapLayers().begin(); i != map->getImageMapLayers().end(); i++ )
    {
        conf.add( i->get()->getConfig() );
        //conf.add( writeLayer( i->get() ) );
    }

    //Write all the heightfield sources
    for (MapLayerList::const_iterator i = map->getHeightFieldMapLayers().begin(); i != map->getHeightFieldMapLayers().end(); i++ )
    {
        conf.add( i->get()->getConfig() );
        //conf.add( writeLayer( i->get() ) );
    }

    //Write all the model layers
    for(ModelLayerList::const_iterator i = map->getModelLayers().begin(); i != map->getModelLayers().end(); i++ )
    {
        conf.add( writeLayer( i->get() ) );
    }

    //Terrain mask layer, if necc.
    if ( map->getTerrainMaskLayer() )
    {
        conf.add( writeLayer( map->getTerrainMaskLayer() ) );
    }

    //NOTE: this is now in MapOptions
	//TODO:  Get this from the getCache call itself, not a CacheConfig.
    //if ( map->cacheConfig().isSet() )
    //{
    //    conf.add( map->cacheConfig()->getConfig( ELEM_CACHE ) );
    //}

    //NOTE: moved to MapNodeOptions
    //if ( map->profileOptions().isSet() )
    //{
    //    conf.add( map->profileOptions()->getConfig() ); // ELEM_PROFILE ) );
    //}

    return conf;
}

bool
EarthFile::readXML( std::istream& input, const std::string& location )
{
    bool success = false;
    osg::ref_ptr<XmlDocument> doc = XmlDocument::load( input );
    if ( doc.valid() )
    {
        Config conf = doc->getConfig().child( ELEM_MAP );

        OE_INFO
            << "[osgEarth] EARTH FILE: " << std::endl
            << conf.toString() << std::endl;

        std::string refURI = location;
        if (!osgDB::containsServerAddress(refURI))
        {
            refURI = osgDB::getRealPath( location );
        }

        success = readMap( conf, refURI, this );
    }
    return success;
}

bool
EarthFile::readXML( const std::string& location )
{
    bool success = false;

    if ( osgDB::containsServerAddress( location ) )
    {
        HTTPResponse response = HTTPClient::get( location );
        if ( response.isOK() && response.getNumParts() > 0 )
        {
            success = readXML( response.getPartStream( 0 ), location );
        }
    }
    else
    {
        if (osgDB::fileExists(location) && (osgDB::fileType(location) == osgDB::REGULAR_FILE))
        {
            std::ifstream in( location.c_str() );
            success = readXML( in, location );
        }
    }

    return success;
}

bool
EarthFile::writeXML( const std::string& location )
{
    if ( !_map.valid() )
        return false;

    std::ofstream out( location.c_str() );
    return writeXML( out );
}

bool
EarthFile::writeXML( std::ostream& output )
{
    if ( !_map.valid() )
        return false;

    Threading::ScopedReadLock lock( _map->getMapDataMutex() );

    Config conf = getConfig( _map.get(), _mapNodeOptions );
    osg::ref_ptr<XmlDocument> doc = new XmlDocument( conf );
    doc->store( output );

    return true;
}

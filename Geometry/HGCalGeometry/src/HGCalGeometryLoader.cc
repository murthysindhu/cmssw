#include "Geometry/HGCalGeometry/interface/HGCalGeometryLoader.h"
#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometry.h"
#include "Geometry/CaloGeometry/interface/FlatTrd.h"
#include "Geometry/HGCalCommonData/interface/HGCalDDDConstants.h"
#include "DataFormats/ForwardDetId/interface/HGCEEDetId.h"
#include "DataFormats/ForwardDetId/interface/HGCHEDetId.h"
#include "DataFormats/ForwardDetId/interface/ForwardSubdetector.h"

//#define DebugLog

typedef CaloCellGeometry::CCGFloat CCGFloat;
typedef std::vector<float> ParmVec;

HGCalGeometryLoader::HGCalGeometryLoader () {}
HGCalGeometryLoader::~HGCalGeometryLoader () {}

HGCalGeometry* HGCalGeometryLoader::build (const HGCalTopology& topology) {

  // allocate geometry
  HGCalGeometry* geom = new HGCalGeometry (topology);
  unsigned int numberOfCells = topology.totalGeomModules(); // both sides
  unsigned int numberExpected= topology.allGeomModules();
#ifdef DebugLog
  std::cout << "Number of Cells " << numberOfCells << ":" << numberExpected
	    << " for sub-detector " << topology.subDetector() 
	    << " Shape parameters " << HGCalGeometry::k_NumberOfShapes << ":" 
	    << HGCalGeometry::k_NumberOfParametersPerShape << std::endl;
#endif
  geom->allocateCorners( numberOfCells ) ;
  geom->allocatePar(HGCalGeometry::k_NumberOfShapes,
		    HGCalGeometry::k_NumberOfParametersPerShape);
  ForwardSubdetector subdet  = topology.subDetector();
  bool               detType = topology.detectorType();

  // loop over modules
  std::vector<HGCalParameters::hgtrform>::const_iterator trItr;
  std::vector<HGCalParameters::hgtrap>::const_iterator   volItr;
  ParmVec params(HGCalGeometry::k_NumberOfParametersPerShape,0);
  unsigned int counter(0);
  for (trItr = topology.dddConstants().getFirstTrForm(); 
       trItr != topology.dddConstants().getLastTrForm(); ++trItr) {
    int zside  = trItr->zp;
    int layer  = trItr->lay;
#ifdef DebugLog
    std::cout << "HGCalGeometryLoader:: Z:Layer:Sector:Subsector " << zside
	      << ":" << layer <<std::endl;
#endif
    if (topology.geomMode() == HGCalGeometryMode::Square) {
      int sector = trItr->sec;
      int subSec = (detType ? trItr->subsec : 0);
      const HepGeom::Transform3D ht3d (trItr->hr, trItr->h3v);
      DetId detId= ((subdet ==  HGCEE) ?
		    (DetId)(HGCEEDetId(subdet,zside,layer,sector,subSec,0)) :
		    (DetId)(HGCHEDetId(subdet,zside,layer,sector,subSec,0)));
#ifdef DebugLog
      std::cout << "HGCalGeometryLoader:: Sector:Subsector " << sector << ":" 
		<< subSec << " transf " << ht3d.getTranslation() << " and " 
		<< ht3d.getRotation();
#endif
      for (volItr = topology.dddConstants().getFirstModule(true);
	   volItr != topology.dddConstants().getLastModule(true); ++volItr) {
	if (volItr->lay == layer) {
	  double alpha = ((detType && subSec == 0) ? -fabs(volItr->alpha) :
			  fabs(volItr->alpha));
	  params[0] = volItr->dz;
	  params[1] = params[2] = 0;
	  params[3] = params[7] = volItr->h;
	  params[4] = params[8] = volItr->bl;
	  params[5] = params[9] = volItr->tl;
	  params[6] = params[10]= alpha;
	  params[11]= volItr->cellSize;

	  buildGeom(params, ht3d, detId, geom);
	  counter++;
	  break;
	}
      }
    } else {
      for (int wafer=0; wafer<topology.dddConstants().sectors(); ++wafer) {
	if (topology.dddConstants().waferInLayer(wafer,layer,true)) {
	  int type = topology.dddConstants().waferTypeT(wafer);
	  if (type != 1) type = 0;
	  DetId detId = (DetId)(HGCalDetId(subdet,zside,layer,type,wafer,0));
	  GlobalPoint w = topology.dddConstants().waferPosition(wafer);
	  double xx = (zside > 0) ? w.x() : -w.x();
	  CLHEP::Hep3Vector h3v(xx,w.y(),trItr->h3v.z());
	  const HepGeom::Transform3D ht3d (trItr->hr, h3v);
#ifdef DebugLog
	  std::cout << "HGCalGeometryLoader:: Wafer:Type " << wafer << ":" 
		    << type << " transf " << ht3d.getTranslation() << " and " 
		    << ht3d.getRotation();
#endif
	  volItr = topology.dddConstants().getModule(wafer);
	  params[0] = volItr->dz;
	  params[1] = params[2] = 0;
	  params[3] = params[7] = volItr->h;
	  params[4] = params[8] = volItr->bl;
	  params[5] = params[9] = volItr->tl;
	  params[6] = params[10]= 0;
	  params[11]= topology.dddConstants().cellSizeHex(type);

	  buildGeom(params, ht3d, detId, geom);
	  counter++;
	}
      }
    }
  }

  geom->sortDetIds();

  if (counter != numberExpected) {
    std::cerr << "inconsistent # of cells: expected " << numberExpected << ":"
	      << numberOfCells << " , inited " << counter << std::endl;
    assert( counter == numberOfCells ) ;
  }

  return geom;
}

void HGCalGeometryLoader::buildGeom(const ParmVec& params, 
				    const HepGeom::Transform3D& ht3d, 
				    const DetId& detId,  HGCalGeometry* geom) {

#ifdef DebugLog
  std::cout << "Volume Parameters";
  for (unsigned int i=0; i<12; ++i) std::cout << " : " << params[i];
  std::cout << std::endl;
#endif
  std::vector<GlobalPoint> corners (8);
	
  FlatTrd::createCorners( params, ht3d, corners ) ;
	
  const CCGFloat* parmPtr (CaloCellGeometry::getParmPtr(params, 
							geom->parMgr(), 
							geom->parVecVec() ) ) ;
	
  GlobalPoint front ( 0.25*( corners[0].x() + 
			     corners[1].x() + 
			     corners[2].x() + 
			     corners[3].x()   ),
		      0.25*( corners[0].y() + 
			     corners[1].y() + 
			     corners[2].y() + 
			     corners[3].y()   ),
		      0.25*( corners[0].z() + 
			     corners[1].z() + 
			     corners[2].z() + 
			     corners[3].z()   ) ) ;
  
  GlobalPoint back  ( 0.25*( corners[4].x() + 
			     corners[5].x() + 
			     corners[6].x() + 
			     corners[7].x()   ),
		      0.25*( corners[4].y() + 
			     corners[5].y() + 
			     corners[6].y() + 
			     corners[7].y()   ),
		      0.25*( corners[4].z() + 
			     corners[5].z() + 
			     corners[6].z() + 
			     corners[7].z()   ) ) ;
  
  if (front.mag2() > back.mag2()) { // front should always point to the center, so swap front and back
    std::swap (front, back);
    std::swap_ranges (corners.begin(), corners.begin()+4, corners.begin()+4); 
  }
	
  geom->newCell(front, back, corners[0], parmPtr, detId) ;
}

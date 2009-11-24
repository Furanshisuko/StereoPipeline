// __BEGIN_LICENSE__
// Copyright (C) 2006-2009 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <vw/FileIO.h>
#include <vw/Image.h>
#include <vw/Cartography.h>
#include <vw/InterestPoint.h>
#include <vw/Math.h>
#include <vw/Mosaic/ImageComposite.h>

using std::cout;
using std::endl;
using std::string;

using namespace vw;
using namespace vw::cartography;
using namespace vw::ip;

// Allows FileIO to correctly read/write these pixel types
namespace vw {
  template<> struct PixelFormatID<Vector3>   { static const PixelFormatEnum value = VW_PIXEL_GENERIC_3_CHANNEL; };
}

// Draw the two images side by side with matching interest points
// shown with lines.
static void write_match_image(std::string out_file_name,
                              std::string const& file1,
                              std::string const& file2,
                              std::vector<InterestPoint> matched_ip1,
                              std::vector<InterestPoint> matched_ip2) {
  // Skip image pairs with no matches.
  if (matched_ip1.size() == 0)
    return;

  DiskImageView<PixelRGB<uint8> > src1(file1);
  DiskImageView<PixelRGB<uint8> > src2(file2);

  mosaic::ImageComposite<PixelRGB<uint8> > composite;
  composite.insert(pixel_cast<PixelRGB<uint8> >(channel_cast_rescale<uint8>(src1.impl())),0,0);
  composite.insert(pixel_cast<PixelRGB<uint8> >(channel_cast_rescale<uint8>(src2.impl())),src1.impl().cols(),0);
  composite.set_draft_mode( true );
  composite.prepare();

  // Rasterize the composite so that we can draw on it.
  ImageView<PixelRGB<uint8> > comp = composite;

  // Draw a red line between matching interest points
  for (unsigned int i = 0; i < matched_ip1.size(); ++i) {
    Vector2 start(matched_ip1[i].x, matched_ip1[i].y);
    Vector2 end(matched_ip2[i].x+src1.impl().cols(), matched_ip2[i].y);
    for (float r=0; r<1.0; r+=1/norm_2(end-start)){
      int i = (int)(0.5 + start.x() + r*(end.x()-start.x()));
      int j = (int)(0.5 + start.y() + r*(end.y()-start.y()));
      if (i >=0 && j >=0 && i < comp.cols() && j < comp.rows())
        comp(i,j) = PixelRGB<uint8>(255, 0, 0);
    }
  }

  write_image(out_file_name, comp, TerminalProgressCallback(InfoMessage, "Writing debug image: "));
}

// Duplicate matches for any given interest point probably indicate a
// poor match, so we cull those out here.
void remove_duplicates(std::vector<InterestPoint> &ip1,
                       std::vector<InterestPoint> &ip2) {
  std::vector<InterestPoint> new_ip1, new_ip2;

  for (unsigned i = 0; i < ip1.size(); ++i) {
    bool bad_entry = false;
    for (unsigned j = 0; j < ip1.size(); ++j) {
      if (i != j &&
          ((ip1[i].x == ip1[j].x && ip1[i].y == ip1[j].y) ||
           (ip2[i].x == ip2[j].x && ip2[i].y == ip2[j].y)) ) {
        bad_entry = true;
      }
    }
    if (!bad_entry) {
      new_ip1.push_back(ip1[i]);
      new_ip2.push_back(ip2[i]);
    }
  }

  ip1 = new_ip1;
  ip2 = new_ip2;
}

void match_orthoimages( string const& left_image_name,
                        string const& right_image_name, 
                        std::vector<InterestPoint> & matched_ip1,
                        std::vector<InterestPoint> & matched_ip2 )
{

  vw_out(0) << "\t--> Finding Interest Points for the orthoimages\n";

  fs::path left_image_path(left_image_name), right_image_path(right_image_name);
  string left_ip_file = change_extension(left_image_path, ".vwip").string();
  string right_ip_file = change_extension(right_image_path, ".vwip").string();
  string match_file = (left_image_path.branch_path() / (fs::basename(left_image_path) + "__" + fs::basename(right_image_path) + ".match")).string();
  
  // Building / Loading Interest point data
  if ( fs::exists(match_file) ) {

    vw_out(0) << "\t    * Using cached match file.\n";
    read_binary_match_file(match_file, matched_ip1, matched_ip2);
    vw_out(0) << "\t    * " << matched_ip1.size() << " matches\n";

  } else {
    std::vector<InterestPoint> ip1_copy, ip2_copy;

    if ( !fs::exists(left_ip_file) ||
         !fs::exists(right_ip_file) ) {

      // Worst case, no interest point operations have been performed before
      vw_out(0) << "\t    * Locating Interest Points\n";
      DiskImageView<PixelGray<float32> > left_disk_image(left_image_name);
      DiskImageView<PixelGray<float32> > right_disk_image(right_image_name);

      // Interest Point module detector code.
      LogInterestOperator log_detector;
      ScaledInterestPointDetector<LogInterestOperator> detector(log_detector, 500);
      std::list<InterestPoint> ip1, ip2;
      vw_out(0) << "\t    * Processing " << left_image_name << "...\n" << std::flush;
      ip1 = detect_interest_points( left_disk_image, detector );
      vw_out(0) << "Located " << ip1.size() << " points.\n";
      vw_out(0) << "\t    * Processing " << right_image_name << "...\n" << std::flush;
      ip2 = detect_interest_points( right_disk_image, detector );
      vw_out(0) << "Located " << ip2.size() << " points.\n";

      vw_out(0) << "\t    * Generating descriptors..." << std::flush;
      PatchDescriptorGenerator descriptor;
      descriptor( left_disk_image, ip1 );
      descriptor( right_disk_image, ip2 );
      vw_out(0) << "done.\n";

      // Writing out the results
      vw_out(0) << "\t    * Caching interest points: "
                << left_ip_file << " & " << right_ip_file << std::endl;
      write_binary_ip_file( left_ip_file, ip1 );
      write_binary_ip_file( right_ip_file, ip2 );

    }
    
    vw_out(0) << "\t    * Using cached IPs.\n";
    ip1_copy = read_binary_ip_file(left_ip_file);
    ip2_copy = read_binary_ip_file(right_ip_file);

    vw_out(0) << "\t    * Matching interest points\n";
    InterestPointMatcher<L2NormMetric,NullConstraint> matcher(0.8);

    matcher(ip1_copy, ip2_copy, matched_ip1, matched_ip2,
            false, TerminalProgressCallback( InfoMessage, "\t    Matching: "));
    
    remove_duplicates(matched_ip1, matched_ip2);
    vw_out(InfoMessage) << "\t    " << matched_ip1.size() << " putative matches.\n";


    vw_out(0) << "\t    * Caching matches: " << match_file << "\n";
    write_binary_match_file( match_file, matched_ip1, matched_ip2);
  }

}

int main( int argc, char *argv[] ) {
  string dem1_name, dem2_name, ortho1_name, ortho2_name, output_prefix;

  po::options_description desc("Options");
  desc.add_options()
    ("help,h", "Display this help message")
    ("dem1", po::value<string>(&dem1_name), "Explicitly specify the first dem")
    ("dem2", po::value<string>(&dem2_name), "Explicitly specify the second dem")
    ("ortho1", po::value<string>(&ortho1_name), "Explicitly specify the first orthoimage")
    ("ortho2", po::value<string>(&ortho2_name), "Explicitly specify the second orthoimage")
    ("output-prefix,o", po::value<string>(&output_prefix), "Specify the output prefix")
    ;

  po::positional_options_description p;
  p.add("ortho1", 1);
  p.add("dem1", 1);
  p.add("ortho2", 1);
  p.add("dem2", 1);
  p.add("output-prefix", 1);

  po::variables_map vm;
  po::store( po::command_line_parser( argc, argv ).options(desc).positional(p).run(), vm );
  po::notify( vm );

  if( vm.count("help") ) {
    cout << desc << endl;
    return 1;
  }

  if( vm.count("dem1") != 1 || vm.count("dem2") != 1 || vm.count("ortho1") != 1 || vm.count("ortho2") != 1) {
    cout << "Usage: " << argv[0] << "ortho1 dem1 ortho2 dem2 output-prefix" << endl;
    cout << desc << endl;
    return 1;
  }

  DiskImageResourceGDAL ortho1_rsrc(ortho1_name), ortho2_rsrc(ortho2_name);

  GeoReference ortho1_georef, ortho2_georef;
  read_georeference(ortho1_georef, ortho1_rsrc);
  read_georeference(ortho2_georef, ortho2_rsrc);


  std::vector<InterestPoint> matched_ip1, matched_ip2;

  match_orthoimages(ortho1_name, ortho2_name, matched_ip1, matched_ip2);
  
  vw_out(0) << "\t--> Rejecting outliers using RANSAC.\n";

  std::vector<Vector3> ransac_ip1, ransac_ip2;

  for (unsigned i = 0; i < matched_ip1.size(); i++) {
    Vector2 loc1 = ortho1_georef.pixel_to_point(Vector2(matched_ip1[i].x, matched_ip1[i].y));
    Vector2 loc2 = ortho2_georef.pixel_to_point(Vector2(matched_ip2[i].x, matched_ip2[i].y));

    loc1 = ortho1_georef.point_to_lonlat(loc1);
    loc2 = ortho2_georef.point_to_lonlat(loc2);

    ransac_ip1.push_back(Vector3(loc1.x(), loc1.y(), 1));
    ransac_ip2.push_back(Vector3(loc2.x(), loc2.y(), 1));
  }

  std::vector<int> indices;
  Matrix<double> trans;
  math::RandomSampleConsensus<math::AffineFittingFunctor,math::InterestPointErrorMetric>
    ransac( math::AffineFittingFunctor(), math::InterestPointErrorMetric(), 0.0001 );
  trans = ransac( ransac_ip1, ransac_ip2 );
  indices = ransac.inlier_indices(trans, ransac_ip1, ransac_ip2 );
  
  vw_out(0) << "\t    * Ransac Result: " << trans << "\n";
  vw_out(0) << "\t                     # inliers: " << indices.size() << "\n";

  return 0;
}
(function ($) {

    Drupal.behaviors.tripal_analysis_interpro = {
        attach: function (context, settings) {

            $("#tripal-interpro-analysis-accordian").accordion({
                collapsible: true,
                active: false,
                heightStyle: "content"
            });


            // For each element of class "interpro-alignment-viewer" add the
            // feature viewer.

            $(".interpro-alignment-viewer").each(function() {

                $(this).css('width', $(this).attr("width"));
                // Create the feature viewer for this analysis.
                var id = $(this).attr('id');

                var analysis_id = $(this).attr('analysis_id');
                var residues = $(this).attr('residues');
                var options = {
                    showAxis: true,
                    showSequence: true,
                    brushActive: true,
                    toolbar: true,
                    bubbleHelp: true,
                    zoomMax: 3
                }
                var fv = new FeatureViewer(residues, '#' + id, options);


                // Add info from data table.

               $(".tripal-analysis-ipr-fv-" + analysis_id).each(function() {

                    var hit_name = $(this).attr('name');
                 //   var class_name = $(this).attr('fv_class');
                    var data = JSON.parse($(this).attr('fv_data'));
                    if (data) {
                        fv.addFeature({
                            data: data,
                            name: hit_name,
                        //    className: class_name,
                            color: "#888888",
                            type: "rect"
                        });
                    }
                })

                fv.onFeatureSelected(function (d) {
                    var id = d.detail.id;
                    var re = /tripal-ipr-domain-(\d+)-(\d+)/;
                    var matches = id.match(re);
                    var analysis_id = matches[1];
                    var j = matches[2];
                    // $(".tripal-analysis-blast-info-hsp-desc").hide();
                    // $("#hsp-desc-" + analysis_id + "-" +j).show();
                });
            });

        }
    }
})(jQuery);

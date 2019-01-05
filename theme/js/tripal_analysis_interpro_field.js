(function ($) {

    Drupal.behaviors.tripal_analysis_blast = {
        attach: function (context, settings) {

            var instance = settings.tripal_analysis_interpro;
            var analysis_id = instance.analysis_id
            var data = instance.data
            var name = instance.name

            $("#tripal-interpro-analysis-accordian").accordion({
                collapsible: true,
                active: false,
                heightStyle: "content"
            });
            // hack to work with multiple jquery versions
            backup_jquery = window.jQuery;
            window.jQuery = $;
            window.$ = $;
            window.ft[analysis_id].addFeature({
                data: data,
                name: name,
                className: "ipr_match",
                color: "#0F8292",
                type: "rect"
            });
            window.jQuery = backup_jquery;
            window.$ = backup_jquery;
        }
    }
})(jQuery);

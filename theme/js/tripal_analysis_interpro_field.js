(function($) {
    // hack to work with multiple jquery versions
    backup_jquery = window.jQuery;
    window.jQuery = $;
    window.$ = $;
    window.ft".$analysis->analysis_id.".addFeature({
        data: ".json_encode($hsp_pos).",
        name: "".$viz_name."",
    className: "ipr_match", //can be used for styling
    color: "#0F8292",
    type: "rect"
});
    window.jQuery = backup_jquery;
    window.$ = backup_jquery;
})(feature_viewer_jquery);
